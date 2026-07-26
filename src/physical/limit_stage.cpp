#include "querylume/physical/limit_stage.h"

namespace querylume {

void LimitStage::onOpen() { child_->open(); }

StageState LimitStage::onGetNext(Row& output) {
    if (emitted_ >= limit_) {
        return StageState::kEof;
    }
    if (child_->getNext(output) == StageState::kEof) {
        return StageState::kEof;
    }
    ++stats_.rows_in;
    ++stats_.rows_out;
    ++emitted_;
    return StageState::kAdvanced;
}

void LimitStage::onClose() noexcept { child_->close(); }

}  // namespace querylume
