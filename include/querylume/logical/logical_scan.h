#pragma once

#include <memory>

#include "querylume/data/table.h"
#include "querylume/logical/logical_plan_node.h"

namespace querylume {

// Leaf node: the in-memory input table. Holds shared_ptr<const Table> (not
// unique) so the same loaded data can back independently-built logical
// plans, e.g. an optimized and an unoptimized plan built from the same
// input for comparison testing.
class LogicalScan final : public LogicalPlanNode {
public:
    explicit LogicalScan(std::shared_ptr<const Table> table) : table_(std::move(table)) {}

    LogicalNodeKind kind() const override { return LogicalNodeKind::kScan; }

    const std::shared_ptr<const Table>& table() const noexcept { return table_; }
    const Schema& outputSchema() const noexcept { return table_->schema; }

private:
    std::shared_ptr<const Table> table_;
};

}  // namespace querylume
