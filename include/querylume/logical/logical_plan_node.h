#pragma once

namespace querylume {

// Logical plan nodes describe WHAT the query computes, independent of HOW
// it is executed (that is the physical plan's job -- see
// querylume/physical). kind() lets consumers (the optimizer, the physical
// planner, EXPLAIN serialization) safely static_cast to the concrete type
// without a virtual-dispatch visitor framework, since there are exactly six
// closed node kinds in the MVP.
enum class LogicalNodeKind {
    kScan,
    kFilter,
    kProject,
    kSort,
    kLimit,
    kTopK,
};

class LogicalPlanNode {
public:
    virtual ~LogicalPlanNode() = default;
    virtual LogicalNodeKind kind() const = 0;
};

}  // namespace querylume
