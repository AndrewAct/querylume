#pragma once

#include <memory>
#include <string>

#include "querylume/logical/logical_plan_node.h"
#include "querylume/optimizer/optimization_context.h"

namespace querylume {

class OptimizerRule {
public:
    virtual ~OptimizerRule() = default;

    // Attempts the rewrite anywhere in the (linear) plan chain rooted at
    // `root`, mutating it in place. Returns true iff at least one rewrite
    // was applied, and always records a RewriteTraceEntry into `context`.
    virtual bool apply(std::unique_ptr<LogicalPlanNode>& root, OptimizationContext& context) const = 0;

    virtual std::string name() const = 0;
};

}  // namespace querylume
