#include "querylume/physical/plan_stage_base.h"

#include <chrono>

#include "querylume/common/error.h"

namespace querylume {

namespace {
using Clock = std::chrono::steady_clock;

std::uint64_t microsSince(Clock::time_point start) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count());
}
}  // namespace

void PlanStageBase::open() {
    if (state_ != LifecycleState::kUnopened) {
        throw QueryLumeError(
            ErrorCode::kInvalidStageLifecycle,
            stageName() + ": open() called more than once (reopen after close is unsupported)");
    }
    ++stats_.open_calls;
    const auto start = Clock::now();
    onOpen();
    stats_.execution_time_micros += microsSince(start);
    state_ = LifecycleState::kOpen;
}

StageState PlanStageBase::getNext(Row& output) {
    if (state_ == LifecycleState::kUnopened || state_ == LifecycleState::kClosed) {
        throw QueryLumeError(ErrorCode::kInvalidStageLifecycle,
                             stageName() + ": getNext() called before open() or after close()");
    }
    ++stats_.get_next_calls;
    if (state_ == LifecycleState::kEof) {
        return StageState::kEof;
    }
    const auto start = Clock::now();
    const StageState result = onGetNext(output);
    stats_.execution_time_micros += microsSince(start);
    if (result == StageState::kEof) {
        state_ = LifecycleState::kEof;
    }
    return result;
}

void PlanStageBase::close() noexcept {
    if (state_ == LifecycleState::kClosed) {
        return;
    }
    onClose();
    state_ = LifecycleState::kClosed;
}

}  // namespace querylume
