#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "querylume/physical/plan_stage_base.h"
#include "querylume/physical/row_comparator.h"

namespace querylume {

// Consumes all child rows while maintaining a bounded heap of at most k
// rows (the current k best under RowComparator), then emits the final rows
// in the same order SortStage + LimitStage would produce.
//
// Complexity: O(n log k) time, O(k) additional space for the heap.
class TopKStage final : public PlanStageBase {
public:
    TopKStage(std::unique_ptr<PlanStage> child, std::size_t column_index, SortDirection direction,
              std::uint64_t k)
        : child_(std::move(child)), comparator_(column_index, direction), k_(k) {}

    std::string stageName() const override { return "TopKStage"; }
    PlanStage* child() const noexcept override { return child_.get(); }

protected:
    void onOpen() override;
    StageState onGetNext(Row& output) override;
    void onClose() noexcept override;

private:
    std::unique_ptr<PlanStage> child_;
    RowComparator comparator_;
    std::uint64_t k_;
    std::vector<Row> buffer_;
    std::size_t next_index_ = 0;
};

}  // namespace querylume
