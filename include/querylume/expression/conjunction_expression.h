#pragma once

#include <memory>
#include <vector>

#include "querylume/expression/expression.h"

namespace querylume {

// Logical AND over one or more operands (implicit AND across $match
// predicates). Short-circuits on the first false operand.
class ConjunctionExpression final : public Expression {
public:
    explicit ConjunctionExpression(std::vector<std::unique_ptr<Expression>> operands)
        : operands_(std::move(operands)) {}

    Value evaluate(const Row& row) const override;
    std::string describe() const override;

private:
    std::vector<std::unique_ptr<Expression>> operands_;
};

}  // namespace querylume
