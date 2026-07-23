#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "querylume/expression/bound_field_expression.h"
#include "querylume/expression/comparison_expression.h"
#include "querylume/expression/conjunction_expression.h"
#include "querylume/expression/literal_expression.h"

namespace querylume {
namespace {

bool evaluateComparison(const Row& row, ComparisonOperator op, Value literal) {
    ComparisonExpression expression(std::make_unique<BoundFieldExpression>(0, "value"), op,
                                    std::make_unique<LiteralExpression>(std::move(literal)));
    return std::get<bool>(expression.evaluate(row).value());
}

TEST(ExpressionTest, ExecutesEverySupportedComparisonOperator) {
    const Row row{0, {std::int64_t{10}}};

    EXPECT_TRUE(evaluateComparison(row, ComparisonOperator::kEq, std::int64_t{10}));
    EXPECT_TRUE(evaluateComparison(row, ComparisonOperator::kNe, std::int64_t{9}));
    EXPECT_TRUE(evaluateComparison(row, ComparisonOperator::kLt, std::int64_t{11}));
    EXPECT_TRUE(evaluateComparison(row, ComparisonOperator::kLte, std::int64_t{10}));
    EXPECT_TRUE(evaluateComparison(row, ComparisonOperator::kGt, std::int64_t{9}));
    EXPECT_TRUE(evaluateComparison(row, ComparisonOperator::kGte, std::int64_t{10}));
}

TEST(ExpressionTest, BoundFieldReturnsBorrowedValueWithoutCopyingString) {
    const Row row{0, {std::string("technology")}};
    const BoundFieldExpression field(0, "sector");

    const EvaluationResult result = field.evaluate(row);
    EXPECT_EQ(&result.value(), &row.values[0]);
}

TEST(ExpressionTest, ConjunctionEvaluatesAllTrueOperands) {
    const Row row{0, {std::int64_t{10}}};
    std::vector<std::unique_ptr<Expression>> operands;
    operands.push_back(std::make_unique<ComparisonExpression>(
        std::make_unique<BoundFieldExpression>(0, "value"), ComparisonOperator::kGte,
        std::make_unique<LiteralExpression>(Value(std::int64_t{10}))));
    operands.push_back(std::make_unique<ComparisonExpression>(
        std::make_unique<BoundFieldExpression>(0, "value"), ComparisonOperator::kLt,
        std::make_unique<LiteralExpression>(Value(std::int64_t{20}))));

    ConjunctionExpression conjunction(std::move(operands));
    EXPECT_TRUE(std::get<bool>(conjunction.evaluate(row).value()));
}

}  // namespace
}  // namespace querylume
