#include "querylume/binder/binder.h"

#include "querylume/common/error.h"
#include "querylume/expression/bound_field_expression.h"
#include "querylume/expression/comparison_expression.h"
#include "querylume/expression/comparison_operator.h"
#include "querylume/expression/conjunction_expression.h"
#include "querylume/expression/literal_expression.h"
#include "querylume/logical/logical_filter.h"
#include "querylume/logical/logical_limit.h"
#include "querylume/logical/logical_project.h"
#include "querylume/logical/logical_scan.h"
#include "querylume/logical/logical_sort.h"

namespace querylume {

namespace {

std::size_t resolveField(const Schema& schema, const std::string& field_name) {
    auto index = schema.indexOf(field_name);
    if (!index.has_value()) {
        throw QueryLumeError(ErrorCode::kUnknownField, "unknown field: '" + field_name + "'");
    }
    return *index;
}

std::unique_ptr<Expression> bindComparison(const Schema& schema, const ParsedComparison& parsed) {
    const std::size_t index = resolveField(schema, parsed.field.field_name);
    const auto op = parseComparisonOperator(parsed.op_token);
    if (!op.has_value()) {
        throw QueryLumeError(ErrorCode::kUnsupportedOperator,
                             "unsupported operator: '" + parsed.op_token + "'");
    }
    auto lhs = std::make_unique<BoundFieldExpression>(index, parsed.field.field_name);
    auto rhs = std::make_unique<LiteralExpression>(parsed.literal);
    return std::make_unique<ComparisonExpression>(std::move(lhs), *op, std::move(rhs));
}

std::unique_ptr<Expression> bindMatch(const Schema& schema, const ParsedMatchStage& match) {
    if (match.predicates.empty()) {
        return std::make_unique<LiteralExpression>(Value(true));
    }
    if (match.predicates.size() == 1) {
        return bindComparison(schema, match.predicates.front());
    }
    std::vector<std::unique_ptr<Expression>> operands;
    operands.reserve(match.predicates.size());
    for (const auto& predicate : match.predicates) {
        operands.push_back(bindComparison(schema, predicate));
    }
    return std::make_unique<ConjunctionExpression>(std::move(operands));
}

SortDirection bindSortDirection(std::int64_t direction_raw) {
    if (direction_raw == 1) return SortDirection::kAscending;
    if (direction_raw == -1) return SortDirection::kDescending;
    throw QueryLumeError(ErrorCode::kInvalidSortDirection,
                         "invalid sort direction: " + std::to_string(direction_raw) + " (must be 1 or -1)");
}

std::uint64_t bindLimit(std::int64_t count_raw) {
    if (count_raw < 0) {
        throw QueryLumeError(ErrorCode::kInvalidLimit,
                             "invalid limit: " + std::to_string(count_raw) + " (must be non-negative)");
    }
    return static_cast<std::uint64_t>(count_raw);
}

}  // namespace

std::unique_ptr<LogicalPlanNode> bindPipeline(std::shared_ptr<const Table> table,
                                              const ParsedPipeline& pipeline) {
    Schema current_schema = table->schema;
    std::unique_ptr<LogicalPlanNode> node = std::make_unique<LogicalScan>(std::move(table));

    for (const ParsedStage& stage : pipeline.stages) {
        switch (stage.kind) {
            case ParsedStageKind::kMatch: {
                auto predicate = bindMatch(current_schema, stage.match);
                node = std::make_unique<LogicalFilter>(std::move(node), std::move(predicate));
                break;
            }
            case ParsedStageKind::kProject: {
                std::vector<std::size_t> indices;
                std::vector<std::string> names;
                indices.reserve(stage.project.fields.size());
                names.reserve(stage.project.fields.size());
                for (const auto& field : stage.project.fields) {
                    indices.push_back(resolveField(current_schema, field.field_name));
                    names.push_back(field.field_name);
                }
                node = std::make_unique<LogicalProject>(std::move(node), indices, names);
                current_schema = Schema(names);
                break;
            }
            case ParsedStageKind::kSort: {
                const std::size_t index = resolveField(current_schema, stage.sort.field.field_name);
                const SortDirection direction = bindSortDirection(stage.sort.direction_raw);
                node = std::make_unique<LogicalSort>(std::move(node), index, stage.sort.field.field_name,
                                                     direction);
                break;
            }
            case ParsedStageKind::kLimit: {
                const std::uint64_t limit = bindLimit(stage.limit.count_raw);
                node = std::make_unique<LogicalLimit>(std::move(node), limit);
                break;
            }
        }
    }

    return node;
}

}  // namespace querylume
