#include "querylume/optimizer/optimizer.h"

#include "querylume/optimizer/sort_limit_to_topk_rule.h"

namespace querylume {

Optimizer::Optimizer() { rules_.push_back(std::make_unique<SortLimitToTopKRule>()); }

OptimizationContext Optimizer::optimize(std::unique_ptr<LogicalPlanNode>& root) const {
    OptimizationContext context;
    for (const auto& rule : rules_) {
        rule->apply(root, context);
    }
    return context;
}

}  // namespace querylume
