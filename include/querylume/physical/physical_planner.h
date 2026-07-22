#pragma once

#include <memory>

#include "querylume/logical/logical_plan_node.h"
#include "querylume/physical/plan_stage.h"

namespace querylume {

// Converts a (typically optimized) logical plan into a PlanStage tree.
// Takes `node` by mutable reference because LogicalFilter's predicate
// Expression is uniquely owned and must be moved into the resulting
// FilterStage (see LogicalFilter::takePredicate()); every other node's
// data is trivially copyable. Throws
// QueryLumeError(kUnsupportedLogicalNode) for any logical node kind this
// planner does not know how to lower (defensive: the current six-kind enum
// is exhaustively handled).
//
// `leaf_scan_out`, if non-null, receives a non-owning pointer to the leaf
// CollectionScanStage once the tree is built, so callers can read its
// rows_out (documentsExamined) after execution without a generic
// tree-search.
std::unique_ptr<PlanStage> buildPhysicalPlan(LogicalPlanNode& node, PlanStage** leaf_scan_out = nullptr);

}  // namespace querylume
