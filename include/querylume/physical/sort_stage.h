#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "querylume/physical/plan_stage_base.h"
#include "querylume/physical/row_comparator.h"

namespace querylume {

// Blocking stage: materializes all child rows, sorts them with the shared
// RowComparator (see row_comparator.h for null/ordinal semantics), then
// streams the sorted rows out one at a time.
//
// Complexity: O(n log n) time, O(n) additional space for the materialized
// row buffer.
class SortStage final : public PlanStageBase {
public:
    SortStage(std::unique_ptr<PlanStage> child, std::size_t column_index, SortDirection direction)
        : child_(std::move(child)), comparator_(column_index, direction) {}

    std::string stageName() const override { return "SortStage"; }
    PlanStage* child() const noexcept override { return child_.get(); }

protected:
    void onOpen() override;
    StageState onGetNext(Row& output) override;
    void onClose() noexcept override;

private:
    std::unique_ptr<PlanStage> child_;
    RowComparator comparator_;
    std::vector<Row> buffer_;
    std::size_t next_index_ = 0;
};

}  // namespace querylume
