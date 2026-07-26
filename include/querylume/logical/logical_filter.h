#pragma once

#include <memory>

#include "querylume/expression/expression.h"
#include "querylume/logical/logical_plan_node.h"

namespace querylume {

class LogicalFilter final : public LogicalPlanNode {
public:
    LogicalFilter(std::unique_ptr<LogicalPlanNode> child, std::unique_ptr<Expression> predicate)
        : child_(std::move(child)), predicate_(std::move(predicate)) {}

    LogicalNodeKind kind() const override { return LogicalNodeKind::kFilter; }

    const LogicalPlanNode& child() const noexcept { return *child_; }
    std::unique_ptr<LogicalPlanNode>& mutableChild() noexcept { return child_; }
    const Expression& predicate() const noexcept { return *predicate_; }
    std::unique_ptr<Expression> takePredicate() { return std::move(predicate_); }

private:
    std::unique_ptr<LogicalPlanNode> child_;
    std::unique_ptr<Expression> predicate_;
};

}  // namespace querylume
