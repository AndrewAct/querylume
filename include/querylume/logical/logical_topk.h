#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "querylume/common/sort_direction.h"
#include "querylume/logical/logical_plan_node.h"

namespace querylume {

// Produced only by SortLimitToTopKRule (see querylume/optimizer). Never
// created directly by the binder.
class LogicalTopK final : public LogicalPlanNode {
public:
    LogicalTopK(std::unique_ptr<LogicalPlanNode> child, std::size_t column_index, std::string field_name,
                SortDirection direction, std::uint64_t k)
        : child_(std::move(child)),
          column_index_(column_index),
          field_name_(std::move(field_name)),
          direction_(direction),
          k_(k) {}

    LogicalNodeKind kind() const override { return LogicalNodeKind::kTopK; }

    const LogicalPlanNode& child() const noexcept { return *child_; }
    std::unique_ptr<LogicalPlanNode>& mutableChild() noexcept { return child_; }
    std::size_t columnIndex() const noexcept { return column_index_; }
    const std::string& fieldName() const noexcept { return field_name_; }
    SortDirection direction() const noexcept { return direction_; }
    std::uint64_t k() const noexcept { return k_; }

private:
    std::unique_ptr<LogicalPlanNode> child_;
    std::size_t column_index_;
    std::string field_name_;
    SortDirection direction_;
    std::uint64_t k_;
};

}  // namespace querylume
