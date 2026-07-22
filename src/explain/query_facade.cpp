#include "querylume/explain/query_facade.h"

#include "querylume/binder/binder.h"
#include "querylume/execution/global_stats.h"
#include "querylume/explain/json_serialize.h"
#include "querylume/logical/logical_plan_utils.h"
#include "querylume/optimizer/optimizer.h"
#include "querylume/parser/pipeline_parser.h"
#include "querylume/physical/physical_planner.h"

namespace querylume {

namespace {

struct PlannedQuery {
    ParsedPipeline parsed;
    std::unique_ptr<LogicalPlanNode> logical_root;
    nlohmann::json logical_plan_json;
    OptimizationContext optimization_context;
    nlohmann::json optimized_logical_plan_json;
    std::unique_ptr<PlanStage> physical_root;
    PlanStage* leaf_scan = nullptr;
    Schema output_schema;
};

PlannedQuery planQuery(const std::shared_ptr<const Table>& table, const nlohmann::json& pipeline_json) {
    PlannedQuery planned;
    planned.parsed = parsePipeline(pipeline_json);
    planned.logical_root = bindPipeline(table, planned.parsed);
    planned.logical_plan_json = toJson(*planned.logical_root);

    Optimizer optimizer;
    planned.optimization_context = optimizer.optimize(planned.logical_root);
    planned.optimized_logical_plan_json = toJson(*planned.logical_root);
    planned.output_schema = outputSchemaOf(*planned.logical_root);

    planned.physical_root = buildPhysicalPlan(*planned.logical_root, &planned.leaf_scan);
    return planned;
}

}  // namespace

QueryResult runPipeline(const std::shared_ptr<const Table>& table, const nlohmann::json& pipeline_json) {
    PlannedQuery planned = planQuery(table, pipeline_json);

    QueryResult result;
    result.output_schema = planned.output_schema;

    planned.physical_root->open();
    Row row;
    while (planned.physical_root->getNext(row) == StageState::kAdvanced) {
        result.rows.push_back(std::move(row));
    }
    planned.physical_root->close();

    return result;
}

nlohmann::json explainPipeline(const std::shared_ptr<const Table>& table,
                               const nlohmann::json& pipeline_json) {
    PlannedQuery planned = planQuery(table, pipeline_json);

    nlohmann::json result;
    result["parsedPipeline"] = toJson(planned.parsed);
    result["logicalPlan"] = planned.logical_plan_json;
    result["optimizedLogicalPlan"] = planned.optimized_logical_plan_json;
    result["rewrites"] = toJson(planned.optimization_context.trace);
    result["physicalPlan"] = toJson(*planned.physical_root, /*include_stats=*/false);
    return result;
}

nlohmann::json explainAnalyzePipeline(const std::shared_ptr<const Table>& table,
                                      const nlohmann::json& pipeline_json, bool include_results) {
    PlannedQuery planned = planQuery(table, pipeline_json);

    planned.physical_root->open();
    std::vector<Row> rows;
    Row row;
    while (planned.physical_root->getNext(row) == StageState::kAdvanced) {
        rows.push_back(std::move(row));
    }
    planned.physical_root->close();

    GlobalStats global;
    global.documents_examined = planned.leaf_scan != nullptr ? planned.leaf_scan->stats().rows_out : 0;
    global.documents_returned = rows.size();
    global.total_execution_time_micros = planned.physical_root->stats().execution_time_micros;
    for (const auto& entry : planned.optimization_context.trace) {
        if (entry.applied) ++global.optimizer_rules_applied;
    }

    nlohmann::json result;
    result["parsedPipeline"] = toJson(planned.parsed);
    result["logicalPlan"] = planned.logical_plan_json;
    result["optimizedLogicalPlan"] = planned.optimized_logical_plan_json;
    result["rewrites"] = toJson(planned.optimization_context.trace);
    result["physicalPlan"] = toJson(*planned.physical_root, /*include_stats=*/true);
    result["globalStats"] = toJson(global);

    if (include_results) {
        nlohmann::json result_rows = nlohmann::json::array();
        for (const auto& r : rows) {
            result_rows.push_back(toJson(r, planned.output_schema));
        }
        result["result"] = std::move(result_rows);
    }

    return result;
}

}  // namespace querylume
