#pragma once

#include <memory>

#include "querylume/expression/comparison_operator.h"
#include "querylume/expression/expression.h"

namespace querylume {

// Evaluates `lhs <op> rhs` and yields a bool Value. Ownership of both
// operands transfers to this node (std::unique_ptr), per the ownership
// requirement for the expression tree.
class ComparisonExpression final : public Expression {
public:
    ComparisonExpression(std::unique_ptr<Expression> lhs, ComparisonOperator op,
                         std::unique_ptr<Expression> rhs)
        : lhs_(std::move(lhs)), op_(op), rhs_(std::move(rhs)) {}

    Value evaluate(const Row& row) const override;
    std::string describe() const override;

private:
    std::unique_ptr<Expression> lhs_;
    ComparisonOperator op_;
    std::unique_ptr<Expression> rhs_;
};

}  // namespace querylume
