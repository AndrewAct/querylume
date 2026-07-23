#pragma once

#include "querylume/physical/plan_stage.h"

namespace querylume {

// RAII guard for one PlanStage execution.
//
// Construction opens the stage tree. Destruction always closes it, including
// when getNext() or result collection throws. This keeps the exception path
// identical to the successful path and prevents callers from accidentally
// forgetting close(). The guard is intentionally neither copyable nor movable:
// exactly one lexical scope owns the open execution.
class PlanStageExecutionGuard final {
public:
    explicit PlanStageExecutionGuard(PlanStage& stage) : stage_(&stage) { stage_->open(); }

    ~PlanStageExecutionGuard() { close(); }

    PlanStageExecutionGuard(const PlanStageExecutionGuard&) = delete;
    PlanStageExecutionGuard& operator=(const PlanStageExecutionGuard&) = delete;
    PlanStageExecutionGuard(PlanStageExecutionGuard&&) = delete;
    PlanStageExecutionGuard& operator=(PlanStageExecutionGuard&&) = delete;

    void close() noexcept {
        if (stage_ == nullptr) return;
        stage_->close();
        stage_ = nullptr;
    }

private:
    PlanStage* stage_;  // Non-owning; the guarded PlanStage must outlive this scope.
};

}  // namespace querylume
