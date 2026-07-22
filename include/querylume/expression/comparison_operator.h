#pragma once

#include <optional>
#include <string>

namespace querylume {

enum class ComparisonOperator {
    kEq,
    kNe,
    kLt,
    kLte,
    kGt,
    kGte,
};

std::string toString(ComparisonOperator op);

// Maps a pipeline operator token ("$eq", "$ne", ...) to ComparisonOperator.
// Returns std::nullopt for anything else (the caller raises
// kUnsupportedOperator).
std::optional<ComparisonOperator> parseComparisonOperator(const std::string& token);

}  // namespace querylume
