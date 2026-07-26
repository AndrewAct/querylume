#pragma once

#include <memory>
#include <vector>

#include "querylume/physical/plan_stage_base.h"

namespace querylume {

// Emits only the columns at `column_indices` (inclusion-only projection,
// resolved once by the binder). Column indices index into the CHILD's row,
// not the output row, so there is no field-name lookup in the hot path.
class ProjectStage final : public PlanStageBase {
public:
    ProjectStage(std::unique_ptr<PlanStage> child, std::vector<std::size_t> column_indices)
        : child_(std::move(child)), column_indices_(std::move(column_indices)) {}

    std::string stageName() const override { return "ProjectStage"; }
    PlanStage* child() const noexcept override { return child_.get(); }

protected:
    void onOpen() override;
    StageState onGetNext(Row& output) override;
    void onClose() noexcept override;

private:
    std::unique_ptr<PlanStage> child_;
    std::vector<std::size_t> column_indices_;
};

}  // namespace querylume
