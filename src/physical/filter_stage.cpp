#include "querylume/physical/filter_stage.h"

namespace querylume {

void FilterStage::onOpen() { child_->open(); }

StageState FilterStage::onGetNext(Row& output) {
    Row candidate;
    while (child_->getNext(candidate) == StageState::kAdvanced) {
        ++stats_.rows_in;
        const Value matched = predicate_->evaluate(candidate);
        if (std::get<bool>(matched)) {
            ++stats_.rows_out;
            output = std::move(candidate);
            return StageState::kAdvanced;
        }
    }
    return StageState::kEof;
}

void FilterStage::onClose() noexcept { child_->close(); }

}  // namespace querylume
