#pragma once

#include <cstdint>
#include <memory>

#include "querylume/physical/plan_stage_base.h"

namespace querylume {

// Emits at most `limit` rows, then EOF without pulling further input. k == 0
// returns EOF on the very first getNext() call without touching the child.
class LimitStage final : public PlanStageBase {
public:
    LimitStage(std::unique_ptr<PlanStage> child, std::uint64_t limit)
        : child_(std::move(child)), limit_(limit) {}

    std::string stageName() const override { return "LimitStage"; }
    PlanStage* child() const noexcept override { return child_.get(); }

protected:
    void onOpen() override;
    StageState onGetNext(Row& output) override;
    void onClose() noexcept override;

private:
    std::unique_ptr<PlanStage> child_;
    std::uint64_t limit_;
    std::uint64_t emitted_ = 0;
};

}  // namespace querylume
