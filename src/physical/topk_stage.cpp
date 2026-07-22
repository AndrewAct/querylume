#include "querylume/physical/topk_stage.h"

#include <algorithm>

#include "querylume/physical/memory_accounting.h"

namespace querylume {

void TopKStage::onOpen() {
    child_->open();

    Row row;
    while (child_->getNext(row) == StageState::kAdvanced) {
        ++stats_.rows_in;
        if (k_ == 0) {
            continue;
        }
        if (buffer_.size() < k_) {
            buffer_.push_back(std::move(row));
            std::push_heap(buffer_.begin(), buffer_.end(), comparator_);
        } else if (comparator_(row, buffer_.front())) {
            std::pop_heap(buffer_.begin(), buffer_.end(), comparator_);
            buffer_.back() = std::move(row);
            std::push_heap(buffer_.begin(), buffer_.end(), comparator_);
        }
        // else: row is not better than the current worst of the top-k; discard.
    }

    std::sort(buffer_.begin(), buffer_.end(), comparator_);
    stats_.peak_memory_bytes = approximateRowsBytes(buffer_);
}

StageState TopKStage::onGetNext(Row& output) {
    if (next_index_ >= buffer_.size()) {
        return StageState::kEof;
    }
    output = std::move(buffer_[next_index_]);
    ++next_index_;
    ++stats_.rows_out;
    return StageState::kAdvanced;
}

void TopKStage::onClose() noexcept { child_->close(); }

}  // namespace querylume
