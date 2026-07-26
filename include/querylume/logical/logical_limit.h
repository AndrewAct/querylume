#pragma once

#include <cstdint>
#include <memory>

#include "querylume/logical/logical_plan_node.h"

namespace querylume {

class LogicalLimit final : public LogicalPlanNode {
public:
    LogicalLimit(std::unique_ptr<LogicalPlanNode> child, std::uint64_t limit)
        : child_(std::move(child)), limit_(limit) {}

    LogicalNodeKind kind() const override { return LogicalNodeKind::kLimit; }

    const LogicalPlanNode& child() const noexcept { return *child_; }
    std::unique_ptr<LogicalPlanNode>& mutableChild() noexcept { return child_; }
    std::uint64_t limit() const noexcept { return limit_; }

private:
    std::unique_ptr<LogicalPlanNode> child_;
    std::uint64_t limit_;
};

}  // namespace querylume
