#include "querylume/data/value.h"

#include "querylume/common/error.h"

namespace querylume {

namespace {

bool isNumeric(ValueType type) { return type == ValueType::kInt64 || type == ValueType::kDouble; }

double asDouble(const Value& value) {
    if (std::holds_alternative<int64_t>(value)) {
        return static_cast<double>(std::get<int64_t>(value));
    }
    return std::get<double>(value);
}

[[noreturn]] void throwIncompatible(ValueType lhs, ValueType rhs) {
    throw QueryLumeError(ErrorCode::kIncompatibleTypes,
                         "cannot compare incompatible types: " + typeName(lhs) + " and " + typeName(rhs));
}

}  // namespace

ValueType typeOf(const Value& value) {
    if (std::holds_alternative<std::nullptr_t>(value)) return ValueType::kNull;
    if (std::holds_alternative<bool>(value)) return ValueType::kBool;
    if (std::holds_alternative<int64_t>(value)) return ValueType::kInt64;
    if (std::holds_alternative<double>(value)) return ValueType::kDouble;
    return ValueType::kString;
}

std::string typeName(ValueType type) {
    switch (type) {
        case ValueType::kNull:
            return "null";
        case ValueType::kBool:
            return "bool";
        case ValueType::kInt64:
            return "int64";
        case ValueType::kDouble:
            return "double";
        case ValueType::kString:
            return "string";
    }
    return "unknown";
}

bool valuesEqual(const Value& lhs, const Value& rhs) {
    const ValueType lt = typeOf(lhs);
    const ValueType rt = typeOf(rhs);

    if (lt == ValueType::kNull && rt == ValueType::kNull) return true;
    if (lt == ValueType::kNull || rt == ValueType::kNull) return false;

    if (isNumeric(lt) && isNumeric(rt)) {
        return asDouble(lhs) == asDouble(rhs);
    }
    if (lt == ValueType::kString && rt == ValueType::kString) {
        return std::get<std::string>(lhs) == std::get<std::string>(rhs);
    }
    if (lt == ValueType::kBool && rt == ValueType::kBool) {
        return std::get<bool>(lhs) == std::get<bool>(rhs);
    }
    throwIncompatible(lt, rt);
}

CompareResult compare(const Value& lhs, const Value& rhs) {
    const ValueType lt = typeOf(lhs);
    const ValueType rt = typeOf(rhs);

    if (lt == ValueType::kNull || rt == ValueType::kNull) {
        return CompareResult::kUnordered;
    }

    if (lt == ValueType::kBool || rt == ValueType::kBool) {
        throwIncompatible(lt, rt);
    }

    if (isNumeric(lt) && isNumeric(rt)) {
        const double a = asDouble(lhs);
        const double b = asDouble(rhs);
        if (a < b) return CompareResult::kLess;
        if (a > b) return CompareResult::kGreater;
        return CompareResult::kEqual;
    }

    if (lt == ValueType::kString && rt == ValueType::kString) {
        const auto& a = std::get<std::string>(lhs);
        const auto& b = std::get<std::string>(rhs);
        if (a < b) return CompareResult::kLess;
        if (a > b) return CompareResult::kGreater;
        return CompareResult::kEqual;
    }

    throwIncompatible(lt, rt);
}

}  // namespace querylume
