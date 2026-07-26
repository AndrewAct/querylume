#include "querylume/logical/logical_plan_utils.h"

#include "querylume/logical/logical_filter.h"
#include "querylume/logical/logical_limit.h"
#include "querylume/logical/logical_project.h"
#include "querylume/logical/logical_scan.h"
#include "querylume/logical/logical_sort.h"
#include "querylume/logical/logical_topk.h"

namespace querylume {

Schema outputSchemaOf(const LogicalPlanNode& node) {
    switch (node.kind()) {
        case LogicalNodeKind::kScan:
            return static_cast<const LogicalScan&>(node).outputSchema();
        case LogicalNodeKind::kProject:
            return Schema(static_cast<const LogicalProject&>(node).fieldNames());
        case LogicalNodeKind::kFilter:
            return outputSchemaOf(static_cast<const LogicalFilter&>(node).child());
        case LogicalNodeKind::kSort:
            return outputSchemaOf(static_cast<const LogicalSort&>(node).child());
        case LogicalNodeKind::kLimit:
            return outputSchemaOf(static_cast<const LogicalLimit&>(node).child());
        case LogicalNodeKind::kTopK:
            return outputSchemaOf(static_cast<const LogicalTopK&>(node).child());
    }
    return Schema();
}

}  // namespace querylume
