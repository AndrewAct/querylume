#pragma once

#include "querylume/expression/expression.h"

namespace querylume {

class LiteralExpression final : public Expression {
public:
    explicit LiteralExpression(Value value) : value_(std::move(value)) {}

    Value evaluate(const Row& /*row*/) const override { return value_; }

    std::string describe() const override;

    const Value& value() const noexcept { return value_; }

private:
    Value value_;
};

}  // namespace querylume
