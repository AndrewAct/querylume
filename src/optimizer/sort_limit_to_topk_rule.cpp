#include "querylume/optimizer/sort_limit_to_topk_rule.h"

#include "querylume/logical/logical_filter.h"
#include "querylume/logical/logical_limit.h"
#include "querylume/logical/logical_project.h"
#include "querylume/logical/logical_scan.h"
#include "querylume/logical/logical_sort.h"
#include "querylume/logical/logical_topk.h"

namespace querylume {

namespace {

std::unique_ptr<LogicalPlanNode>* childSlot(LogicalPlanNode* node) {
    switch (node->kind()) {
        case LogicalNodeKind::kScan:
            return nullptr;
        case LogicalNodeKind::kFilter:
            return &static_cast<LogicalFilter*>(node)->mutableChild();
        case LogicalNodeKind::kProject:
            return &static_cast<LogicalProject*>(node)->mutableChild();
        case LogicalNodeKind::kSort:
            return &static_cast<LogicalSort*>(node)->mutableChild();
        case LogicalNodeKind::kLimit:
            return &static_cast<LogicalLimit*>(node)->mutableChild();
        case LogicalNodeKind::kTopK:
            return &static_cast<LogicalTopK*>(node)->mutableChild();
    }
    return nullptr;
}

}  // namespace

bool SortLimitToTopKRule::apply(std::unique_ptr<LogicalPlanNode>& root, OptimizationContext& context) const {
    bool applied_any = false;
    std::unique_ptr<LogicalPlanNode>* current = &root;

    while (current != nullptr && *current != nullptr) {
        LogicalPlanNode* node = current->get();

        if (node->kind() == LogicalNodeKind::kLimit) {
            auto* limit_node = static_cast<LogicalLimit*>(node);
            LogicalPlanNode* child_node = limit_node->mutableChild().get();
            if (child_node != nullptr && child_node->kind() == LogicalNodeKind::kSort) {
                auto* sort_node = static_cast<LogicalSort*>(child_node);
                std::unique_ptr<LogicalPlanNode> grandchild = std::move(sort_node->mutableChild());
                auto new_topk = std::make_unique<LogicalTopK>(std::move(grandchild), sort_node->columnIndex(),
                                                              sort_node->fieldName(), sort_node->direction(),
                                                              limit_node->limit());
                *current = std::move(new_topk);
                applied_any = true;
                current = &static_cast<LogicalTopK*>(current->get())->mutableChild();
                continue;
            }
        }

        current = childSlot(current->get());
    }

    context.trace.push_back({name(), applied_any});
    return applied_any;
}

}  // namespace querylume
