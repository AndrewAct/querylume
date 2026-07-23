#include "querylume/explain/json_serialize.h"

#include "querylume/common/sort_direction.h"
#include "querylume/logical/logical_filter.h"
#include "querylume/logical/logical_limit.h"
#include "querylume/logical/logical_project.h"
#include "querylume/logical/logical_scan.h"
#include "querylume/logical/logical_sort.h"
#include "querylume/logical/logical_topk.h"

namespace querylume {

namespace {
int directionToInt(SortDirection direction) { return direction == SortDirection::kAscending ? 1 : -1; }
}  // namespace

nlohmann::json toJson(const Value& value) {
    switch (typeOf(value)) {
        case ValueType::kNull:
            return nullptr;
        case ValueType::kBool:
            return std::get<bool>(value);
        case ValueType::kInt64:
            return std::get<int64_t>(value);
        case ValueType::kDouble:
            return std::get<double>(value);
        case ValueType::kString:
            return std::get<std::string>(value);
    }
    return nullptr;
}

nlohmann::json toJson(const Row& row, const Schema& schema) {
    nlohmann::json obj = nlohmann::json::object();
    const auto& fields = schema.fields();
    for (std::size_t i = 0; i < fields.size() && i < row.values.size(); ++i) {
        obj[fields[i]] = toJson(row.values[i]);
    }
    return obj;
}

nlohmann::json toJson(const ParsedPipeline& pipeline) {
    nlohmann::json stages = nlohmann::json::array();
    for (const auto& stage : pipeline.stages) {
        nlohmann::json entry;
        switch (stage.kind) {
            case ParsedStageKind::kMatch: {
                entry["stage"] = "$match";
                nlohmann::json predicates = nlohmann::json::array();
                for (const auto& predicate : stage.match.predicates) {
                    predicates.push_back({{"field", predicate.field.field_name},
                                          {"operator", predicate.op_token},
                                          {"literal", toJson(predicate.literal)}});
                }
                entry["predicates"] = std::move(predicates);
                break;
            }
            case ParsedStageKind::kProject: {
                entry["stage"] = "$project";
                nlohmann::json fields = nlohmann::json::array();
                for (const auto& field : stage.project.fields) {
                    fields.push_back(field.field_name);
                }
                entry["fields"] = std::move(fields);
                break;
            }
            case ParsedStageKind::kSort: {
                entry["stage"] = "$sort";
                entry["field"] = stage.sort.field.field_name;
                entry["direction"] = stage.sort.direction_raw;
                break;
            }
            case ParsedStageKind::kLimit: {
                entry["stage"] = "$limit";
                entry["count"] = stage.limit.count_raw;
                break;
            }
        }
        stages.push_back(std::move(entry));
    }
    return stages;
}

nlohmann::json toJson(const LogicalPlanNode& node) {
    nlohmann::json j;
    switch (node.kind()) {
        case LogicalNodeKind::kScan: {
            const auto& scan = static_cast<const LogicalScan&>(node);
            j["node"] = "Scan";
            j["fields"] = scan.outputSchema().fields();
            j["rowCount"] = scan.table()->rows.size();
            break;
        }
        case LogicalNodeKind::kFilter: {
            const auto& filter = static_cast<const LogicalFilter&>(node);
            j["node"] = "Filter";
            j["predicate"] = filter.predicate().describe();
            j["child"] = toJson(filter.child());
            break;
        }
        case LogicalNodeKind::kProject: {
            const auto& project = static_cast<const LogicalProject&>(node);
            j["node"] = "Project";
            j["fields"] = project.fieldNames();
            j["child"] = toJson(project.child());
            break;
        }
        case LogicalNodeKind::kSort: {
            const auto& sort = static_cast<const LogicalSort&>(node);
            j["node"] = "Sort";
            j["field"] = sort.fieldName();
            j["direction"] = directionToInt(sort.direction());
            j["child"] = toJson(sort.child());
            break;
        }
        case LogicalNodeKind::kLimit: {
            const auto& limit = static_cast<const LogicalLimit&>(node);
            j["node"] = "Limit";
            j["count"] = limit.limit();
            j["child"] = toJson(limit.child());
            break;
        }
        case LogicalNodeKind::kTopK: {
            const auto& topk = static_cast<const LogicalTopK&>(node);
            j["node"] = "TopK";
            j["field"] = topk.fieldName();
            j["direction"] = directionToInt(topk.direction());
            j["k"] = topk.k();
            j["child"] = toJson(topk.child());
            break;
        }
    }
    return j;
}

nlohmann::json toJson(const std::vector<RewriteTraceEntry>& trace) {
    nlohmann::json entries = nlohmann::json::array();
    for (const auto& entry : trace) {
        entries.push_back({{"rule", entry.rule}, {"applied", entry.applied}});
    }
    return entries;
}

nlohmann::json toJson(const PlanStage& stage, bool include_stats) {
    nlohmann::json j;
    j["stage"] = stage.stageName();
    if (include_stats) {
        const StageStats& s = stage.stats();
        j["stats"] = {
            {"rowsIn", s.rows_in},
            {"rowsOut", s.rows_out},
            {"openCalls", s.open_calls},
            {"getNextCalls", s.get_next_calls},
            {"executionTimeMicros", s.execution_time_micros},
            {"peakMemoryBytes", s.peak_memory_bytes},
        };
    }
    if (PlanStage* child = stage.child()) {
        j["child"] = toJson(*child, include_stats);
    }
    return j;
}

nlohmann::json toJson(const GlobalStats& stats) {
    return {
        {"documentsExamined", stats.documents_examined},
        {"documentsReturned", stats.documents_returned},
        {"totalExecutionTimeMicros", stats.total_execution_time_micros},
        {"optimizerRulesApplied", stats.optimizer_rules_applied},
    };
}

}  // namespace querylume
