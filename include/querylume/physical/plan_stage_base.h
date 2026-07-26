#pragma once

#include "querylume/physical/plan_stage.h"

namespace querylume {

// Shared lifecycle enforcement + stats/timing bookkeeping for all concrete
// PlanStage implementations. Subclasses implement the on*() hooks instead of
// open()/getNext()/close() directly, so lifecycle-contract checks and timing
// live in exactly one place.
class PlanStageBase : public PlanStage {
public:
    void open() final;
    StageState getNext(Row& output) final;
    void close() noexcept final;

    const StageStats& stats() const noexcept final { return stats_; }

protected:
    virtual void onOpen() = 0;
    virtual StageState onGetNext(Row& output) = 0;
    virtual void onClose() noexcept = 0;

    StageStats stats_;

private:
    enum class LifecycleState { kUnopened, kOpen, kClosed, kEof };
    LifecycleState state_ = LifecycleState::kUnopened;
};

}  // namespace querylume
