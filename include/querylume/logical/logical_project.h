#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "querylume/logical/logical_plan_node.h"

namespace querylume {

// Inclusion-only projection. `column_indices` index into the child's output
// schema; `field_names` are the corresponding output field names in
// requested order (this becomes the schema visible to any stage above).
class LogicalProject final : public LogicalPlanNode {
public:
    LogicalProject(std::unique_ptr<LogicalPlanNode> child, std::vector<std::size_t> column_indices,
                   std::vector<std::string> field_names)
        : child_(std::move(child)),
          column_indices_(std::move(column_indices)),
          field_names_(std::move(field_names)) {}

    LogicalNodeKind kind() const override { return LogicalNodeKind::kProject; }

    const LogicalPlanNode& child() const noexcept { return *child_; }
    std::unique_ptr<LogicalPlanNode>& mutableChild() noexcept { return child_; }
    const std::vector<std::size_t>& columnIndices() const noexcept { return column_indices_; }
    const std::vector<std::string>& fieldNames() const noexcept { return field_names_; }

private:
    std::unique_ptr<LogicalPlanNode> child_;
    std::vector<std::size_t> column_indices_;
    std::vector<std::string> field_names_;
};

}  // namespace querylume
