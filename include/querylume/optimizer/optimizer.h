#pragma once

#include <memory>
#include <vector>

#include "querylume/optimizer/optimization_context.h"
#include "querylume/optimizer/optimizer_rule.h"

namespace querylume {

// Runs the registered rule set once, in order, against a logical plan. The
// MVP registers exactly one rule (SortLimitToTopKRule); the rule-list
// structure exists so adding a rule later doesn't require touching the
// driver, not as an invitation to add more rules now.
class Optimizer {
public:
    Optimizer();

    OptimizationContext optimize(std::unique_ptr<LogicalPlanNode>& root) const;

private:
    std::vector<std::unique_ptr<OptimizerRule>> rules_;
};

}  // namespace querylume
