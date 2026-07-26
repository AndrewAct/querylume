#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "querylume/common/sort_direction.h"
#include "querylume/logical/logical_plan_node.h"

namespace querylume {

class LogicalSort final : public LogicalPlanNode {
public:
    LogicalSort(std::unique_ptr<LogicalPlanNode> child, std::size_t column_index, std::string field_name,
                SortDirection direction)
        : child_(std::move(child)),
          column_index_(column_index),
          field_name_(std::move(field_name)),
          direction_(direction) {}

    LogicalNodeKind kind() const override { return LogicalNodeKind::kSort; }

    const LogicalPlanNode& child() const noexcept { return *child_; }
    std::unique_ptr<LogicalPlanNode>& mutableChild() noexcept { return child_; }
    std::size_t columnIndex() const noexcept { return column_index_; }
    const std::string& fieldName() const noexcept { return field_name_; }
    SortDirection direction() const noexcept { return direction_; }

private:
    std::unique_ptr<LogicalPlanNode> child_;
    std::size_t column_index_;
    std::string field_name_;
    SortDirection direction_;
};

}  // namespace querylume
