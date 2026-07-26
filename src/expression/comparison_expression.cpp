#include "querylume/expression/comparison_expression.h"

#include "querylume/common/error.h"

namespace querylume {

EvaluationResult ComparisonExpression::evaluate(const Row& row) const {
    const EvaluationResult lhs_result = lhs_->evaluate(row);
    const EvaluationResult rhs_result = rhs_->evaluate(row);
    const Value& lhs = lhs_result.value();
    const Value& rhs = rhs_result.value();

    switch (op_) {
        case ComparisonOperator::kEq:
            return EvaluationResult(Value(valuesEqual(lhs, rhs)));
        case ComparisonOperator::kNe:
            return EvaluationResult(Value(!valuesEqual(lhs, rhs)));
        case ComparisonOperator::kLt: {
            const CompareResult r = compare(lhs, rhs);
            return EvaluationResult(Value(r == CompareResult::kLess));
        }
        case ComparisonOperator::kLte: {
            const CompareResult r = compare(lhs, rhs);
            return EvaluationResult(Value(r == CompareResult::kLess || r == CompareResult::kEqual));
        }
        case ComparisonOperator::kGt: {
            const CompareResult r = compare(lhs, rhs);
            return EvaluationResult(Value(r == CompareResult::kGreater));
        }
        case ComparisonOperator::kGte: {
            const CompareResult r = compare(lhs, rhs);
            return EvaluationResult(Value(r == CompareResult::kGreater || r == CompareResult::kEqual));
        }
    }
    throw QueryLumeError(ErrorCode::kUnsupportedOperator, "unsupported comparison operator");
}

std::string ComparisonExpression::describe() const {
    return lhs_->describe() + " " + toString(op_) + " " + rhs_->describe();
}

}  // namespace querylume
