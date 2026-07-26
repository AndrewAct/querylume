#include "querylume/physical/project_stage.h"

namespace querylume {

void ProjectStage::onOpen() { child_->open(); }

StageState ProjectStage::onGetNext(Row& output) {
    Row input;
    if (child_->getNext(input) == StageState::kEof) {
        return StageState::kEof;
    }
    ++stats_.rows_in;

    Row projected;
    projected.ordinal = input.ordinal;
    projected.values.reserve(column_indices_.size());
    for (const std::size_t index : column_indices_) {
        projected.values.push_back(std::move(input.values[index]));
    }
    output = std::move(projected);
    ++stats_.rows_out;
    return StageState::kAdvanced;
}

void ProjectStage::onClose() noexcept { child_->close(); }

}  // namespace querylume
