#pragma once

#include "querylume/optimizer/optimizer_rule.h"

namespace querylume {

// Rewrites every occurrence, anywhere in the plan chain, of:
//   LogicalLimit(k)
//     LogicalSort(field, direction)
//       Child
// into:
//   LogicalTopK(field, direction, k)
//     Child
//
// Only fires when a Limit's DIRECT child is a Sort (non-adjacent Sort/Limit
// pairs, e.g. separated by a Filter or Project, are left untouched).
class SortLimitToTopKRule final : public OptimizerRule {
public:
    bool apply(std::unique_ptr<LogicalPlanNode>& root, OptimizationContext& context) const override;
    std::string name() const override { return "SortLimitToTopK"; }
};

}  // namespace querylume
