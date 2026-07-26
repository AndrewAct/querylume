#pragma once

#include <stdexcept>
#include <string>

namespace querylume {

enum class ErrorCode {
    kJsonParseError,
    kInvalidDataShape,
    kUnsupportedValueType,
    kPipelineSyntaxError,
    kUnknownStage,
    kUnknownField,
    kUnsupportedOperator,
    kIncompatibleTypes,
    kInvalidSortDirection,
    kInvalidLimit,
    kInvalidStageLifecycle,
    kUnsupportedLogicalNode,
};

// Thrown for any user-input validation failure (malformed JSON, invalid
// pipeline syntax, unknown fields/operators, etc). Assertions are reserved
// for internal invariants only; all user-facing failures surface as
// QueryLumeError so the CLI can report a clean message and exit non-zero.
class QueryLumeError : public std::runtime_error {
public:
    QueryLumeError(ErrorCode code, const std::string& message) : std::runtime_error(message), code_(code) {}

    ErrorCode code() const noexcept { return code_; }

private:
    ErrorCode code_;
};

std::string toString(ErrorCode code);

}  // namespace querylume
