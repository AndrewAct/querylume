#include "querylume/expression/literal_expression.h"

#include <sstream>

namespace querylume {

std::string LiteralExpression::describe() const {
    switch (typeOf(value_)) {
        case ValueType::kNull:
            return "null";
        case ValueType::kBool:
            return std::get<bool>(value_) ? "true" : "false";
        case ValueType::kInt64:
            return std::to_string(std::get<int64_t>(value_));
        case ValueType::kDouble: {
            std::ostringstream oss;
            oss << std::get<double>(value_);
            return oss.str();
        }
        case ValueType::kString:
            return "\"" + std::get<std::string>(value_) + "\"";
    }
    return "?";
}

}  // namespace querylume
