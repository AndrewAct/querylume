#include "querylume/expression/conjunction_expression.h"

namespace querylume {

EvaluationResult ConjunctionExpression::evaluate(const Row& row) const {
    for (const auto& operand : operands_) {
        const EvaluationResult result = operand->evaluate(row);
        if (!std::get<bool>(result.value())) {
            return EvaluationResult(Value(false));
        }
    }
    return EvaluationResult(Value(true));
}

std::string ConjunctionExpression::describe() const {
    std::string result;
    for (std::size_t i = 0; i < operands_.size(); ++i) {
        if (i > 0) result += " AND ";
        result += operands_[i]->describe();
    }
    return result.empty() ? "true" : result;
}

}  // namespace querylume
