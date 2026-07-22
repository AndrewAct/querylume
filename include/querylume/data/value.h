#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace querylume {

// A scalar cell value. QueryLume only supports flat scalar documents; nested
// objects and arrays are rejected during schema inference (see schema.h).
using Value = std::variant<std::nullptr_t, bool, int64_t, double, std::string>;

enum class ValueType {
    kNull,
    kBool,
    kInt64,
    kDouble,
    kString,
};

ValueType typeOf(const Value& value);
std::string typeName(ValueType type);

// Three-valued comparison result for ordered comparisons.
enum class CompareResult {
    kLess,
    kEqual,
    kGreater,
    // Ordered comparison (<, <=, >, >=) against a null operand. Per spec,
    // such comparisons always evaluate to false; kUnordered signals the
    // caller to short-circuit rather than trust Less/Equal/Greater.
    kUnordered,
};

// Value comparison semantics (see docs/decisions.md and docs/architecture.md
// for rationale):
//   - int64_t and double compare numerically (mixed comparisons allowed).
//   - strings compare lexicographically (byte-wise).
//   - bools support only equality/inequality; ordered comparison is
//     kUnordered.
//   - null == null is true; null compared for equality with a non-null value
//     is false; any ordered comparison touching null is kUnordered.
//   - incompatible non-numeric type pairs (e.g. string vs int64_t) throw
//     QueryLumeError(kIncompatibleTypes).
CompareResult compare(const Value& lhs, const Value& rhs);
bool valuesEqual(const Value& lhs, const Value& rhs);

}  // namespace querylume
