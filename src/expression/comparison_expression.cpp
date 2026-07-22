#include "querylume/expression/comparison_expression.h"

#include "querylume/common/error.h"

namespace querylume {

Value ComparisonExpression::evaluate(const Row& row) const {
    const Value lhs = lhs_->evaluate(row);
    const Value rhs = rhs_->evaluate(row);

    switch (op_) {
        case ComparisonOperator::kEq:
            return valuesEqual(lhs, rhs);
        case ComparisonOperator::kNe:
            return !valuesEqual(lhs, rhs);
        case ComparisonOperator::kLt: {
            const CompareResult r = compare(lhs, rhs);
            return r == CompareResult::kLess;
        }
        case ComparisonOperator::kLte: {
            const CompareResult r = compare(lhs, rhs);
            return r == CompareResult::kLess || r == CompareResult::kEqual;
        }
        case ComparisonOperator::kGt: {
            const CompareResult r = compare(lhs, rhs);
            return r == CompareResult::kGreater;
        }
        case ComparisonOperator::kGte: {
            const CompareResult r = compare(lhs, rhs);
            return r == CompareResult::kGreater || r == CompareResult::kEqual;
        }
    }
    throw QueryLumeError(ErrorCode::kUnsupportedOperator, "unsupported comparison operator");
}

std::string ComparisonExpression::describe() const {
    return lhs_->describe() + " " + toString(op_) + " " + rhs_->describe();
}

}  // namespace querylume
