#include "querylume/expression/comparison_operator.h"

namespace querylume {

std::string toString(ComparisonOperator op) {
    switch (op) {
        case ComparisonOperator::kEq:
            return "$eq";
        case ComparisonOperator::kNe:
            return "$ne";
        case ComparisonOperator::kLt:
            return "$lt";
        case ComparisonOperator::kLte:
            return "$lte";
        case ComparisonOperator::kGt:
            return "$gt";
        case ComparisonOperator::kGte:
            return "$gte";
    }
    return "?";
}

std::optional<ComparisonOperator> parseComparisonOperator(const std::string& token) {
    if (token == "$eq") return ComparisonOperator::kEq;
    if (token == "$ne") return ComparisonOperator::kNe;
    if (token == "$lt") return ComparisonOperator::kLt;
    if (token == "$lte") return ComparisonOperator::kLte;
    if (token == "$gt") return ComparisonOperator::kGt;
    if (token == "$gte") return ComparisonOperator::kGte;
    return std::nullopt;
}

}  // namespace querylume
