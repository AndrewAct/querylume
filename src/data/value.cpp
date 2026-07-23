#include "querylume/data/value.h"

#include <cmath>

#include "querylume/common/error.h"

namespace querylume {

namespace {

bool isNumeric(ValueType type) { return type == ValueType::kInt64 || type == ValueType::kDouble; }

[[noreturn]] void throwUnsupportedNumber() {
    throw QueryLumeError(ErrorCode::kUnsupportedValueType, "NaN cannot participate in comparisons");
}

CompareResult compareDoubles(double lhs, double rhs) {
    if (std::isnan(lhs) || std::isnan(rhs)) {
        throwUnsupportedNumber();
    }
    if (lhs < rhs) return CompareResult::kLess;
    if (lhs > rhs) return CompareResult::kGreater;
    return CompareResult::kEqual;
}

// Comparing by first casting int64_t to double loses integer precision above
// 2^53. Instead, range-check the double, truncate only when conversion is
// defined, then use its fractional part to finish the comparison exactly.
CompareResult compareIntToDouble(std::int64_t integer, double floating) {
    if (std::isnan(floating)) {
        throwUnsupportedNumber();
    }

    constexpr double kInt64LowerBound = -9223372036854775808.0;  // -2^63, exactly representable.
    constexpr double kInt64UpperBound = 9223372036854775808.0;   //  2^63, one past INT64_MAX.
    if (floating < kInt64LowerBound) return CompareResult::kGreater;
    if (floating >= kInt64UpperBound) return CompareResult::kLess;

    const auto truncated = static_cast<std::int64_t>(floating);
    if (integer < truncated) return CompareResult::kLess;
    if (integer > truncated) return CompareResult::kGreater;
    if (floating > static_cast<double>(truncated)) return CompareResult::kLess;
    if (floating < static_cast<double>(truncated)) return CompareResult::kGreater;
    return CompareResult::kEqual;
}

CompareResult invert(CompareResult result) {
    if (result == CompareResult::kLess) return CompareResult::kGreater;
    if (result == CompareResult::kGreater) return CompareResult::kLess;
    return result;
}

CompareResult compareNumeric(const Value& lhs, const Value& rhs) {
    const bool lhs_is_integer = std::holds_alternative<std::int64_t>(lhs);
    const bool rhs_is_integer = std::holds_alternative<std::int64_t>(rhs);

    if (lhs_is_integer && rhs_is_integer) {
        const auto a = std::get<std::int64_t>(lhs);
        const auto b = std::get<std::int64_t>(rhs);
        if (a < b) return CompareResult::kLess;
        if (a > b) return CompareResult::kGreater;
        return CompareResult::kEqual;
    }
    if (!lhs_is_integer && !rhs_is_integer) {
        return compareDoubles(std::get<double>(lhs), std::get<double>(rhs));
    }
    if (lhs_is_integer) {
        return compareIntToDouble(std::get<std::int64_t>(lhs), std::get<double>(rhs));
    }
    return invert(compareIntToDouble(std::get<std::int64_t>(rhs), std::get<double>(lhs)));
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
        return compareNumeric(lhs, rhs) == CompareResult::kEqual;
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
        return compareNumeric(lhs, rhs);
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
