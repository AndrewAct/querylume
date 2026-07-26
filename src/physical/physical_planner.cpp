#include "querylume/physical/physical_planner.h"

#include "querylume/common/error.h"
#include "querylume/logical/logical_filter.h"
#include "querylume/logical/logical_limit.h"
#include "querylume/logical/logical_project.h"
#include "querylume/logical/logical_scan.h"
#include "querylume/logical/logical_sort.h"
#include "querylume/logical/logical_topk.h"
#include "querylume/physical/collection_scan_stage.h"
#include "querylume/physical/filter_stage.h"
#include "querylume/physical/limit_stage.h"
#include "querylume/physical/project_stage.h"
#include "querylume/physical/sort_stage.h"
#include "querylume/physical/topk_stage.h"

namespace querylume {

std::unique_ptr<PlanStage> buildPhysicalPlan(std::unique_ptr<LogicalPlanNode> node,
                                             PlanStage** leaf_scan_out) {
    if (node == nullptr) {
        throw QueryLumeError(ErrorCode::kUnsupportedLogicalNode,
                             "physical planner cannot lower a null logical plan");
    }

    // Logical nodes describe WHAT should happen. This switch is the single
    // boundary that chooses HOW each operation runs. Recursing with moved
    // child unique_ptrs consumes the logical tree from the root downward and
    // constructs a new, independently-owned physical execution tree.
    switch (node->kind()) {
        case LogicalNodeKind::kScan: {
            auto& scan = static_cast<LogicalScan&>(*node);
            auto stage = std::make_unique<CollectionScanStage>(scan.table());
            if (leaf_scan_out != nullptr) {
                *leaf_scan_out = stage.get();
            }
            return stage;
        }
        case LogicalNodeKind::kFilter: {
            auto& filter = static_cast<LogicalFilter&>(*node);
            auto child_stage = buildPhysicalPlan(std::move(filter.mutableChild()), leaf_scan_out);
            return std::make_unique<FilterStage>(std::move(child_stage), filter.takePredicate());
        }
        case LogicalNodeKind::kProject: {
            auto& project = static_cast<LogicalProject&>(*node);
            auto child_stage = buildPhysicalPlan(std::move(project.mutableChild()), leaf_scan_out);
            return std::make_unique<ProjectStage>(std::move(child_stage), project.columnIndices());
        }
        case LogicalNodeKind::kSort: {
            auto& sort = static_cast<LogicalSort&>(*node);
            auto child_stage = buildPhysicalPlan(std::move(sort.mutableChild()), leaf_scan_out);
            return std::make_unique<SortStage>(std::move(child_stage), sort.columnIndex(), sort.direction());
        }
        case LogicalNodeKind::kLimit: {
            auto& limit = static_cast<LogicalLimit&>(*node);
            auto child_stage = buildPhysicalPlan(std::move(limit.mutableChild()), leaf_scan_out);
            return std::make_unique<LimitStage>(std::move(child_stage), limit.limit());
        }
        case LogicalNodeKind::kTopK: {
            auto& topk = static_cast<LogicalTopK&>(*node);
            auto child_stage = buildPhysicalPlan(std::move(topk.mutableChild()), leaf_scan_out);
            return std::make_unique<TopKStage>(std::move(child_stage), topk.columnIndex(), topk.direction(),
                                               topk.k());
        }
    }
    throw QueryLumeError(ErrorCode::kUnsupportedLogicalNode,
                         "physical planner cannot lower this logical node");
}

}  // namespace querylume
