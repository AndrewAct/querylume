#pragma once

#include <memory>

#include "querylume/logical/logical_plan_node.h"
#include "querylume/physical/plan_stage.h"

namespace querylume {

// Consumes a (typically optimized) logical plan and converts it into a
// PlanStage tree. Taking unique_ptr by value makes the ownership transition
// explicit at the API boundary: after this call, callers no longer have a
// logical tree whose uniquely-owned expressions have been moved elsewhere.
// Throws
// QueryLumeError(kUnsupportedLogicalNode) for any logical node kind this
// planner does not know how to lower (defensive: the current six-kind enum
// is exhaustively handled).
//
// `leaf_scan_out`, if non-null, receives a non-owning pointer to the leaf
// CollectionScanStage once the tree is built, so callers can read its
// rows_out (documentsExamined) after execution without a generic
// tree-search.
std::unique_ptr<PlanStage> buildPhysicalPlan(std::unique_ptr<LogicalPlanNode> node,
                                             PlanStage** leaf_scan_out = nullptr);

}  // namespace querylume
