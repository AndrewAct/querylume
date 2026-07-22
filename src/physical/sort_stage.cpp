#include "querylume/physical/sort_stage.h"

#include <algorithm>

#include "querylume/physical/memory_accounting.h"

namespace querylume {

void SortStage::onOpen() {
    child_->open();
    Row row;
    while (child_->getNext(row) == StageState::kAdvanced) {
        ++stats_.rows_in;
        buffer_.push_back(std::move(row));
    }
    std::sort(buffer_.begin(), buffer_.end(), comparator_);
    stats_.peak_memory_bytes = approximateRowsBytes(buffer_);
}

StageState SortStage::onGetNext(Row& output) {
    if (next_index_ >= buffer_.size()) {
        return StageState::kEof;
    }
    output = std::move(buffer_[next_index_]);
    ++next_index_;
    ++stats_.rows_out;
    return StageState::kAdvanced;
}

void SortStage::onClose() noexcept { child_->close(); }

}  // namespace querylume
