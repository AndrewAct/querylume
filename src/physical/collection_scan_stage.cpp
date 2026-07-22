#include "querylume/physical/collection_scan_stage.h"

namespace querylume {

CollectionScanStage::CollectionScanStage(std::shared_ptr<const Table> table) : table_(std::move(table)) {}

void CollectionScanStage::onOpen() { next_index_ = 0; }

StageState CollectionScanStage::onGetNext(Row& output) {
    if (next_index_ >= table_->rows.size()) {
        return StageState::kEof;
    }
    output = table_->rows[next_index_];
    ++next_index_;
    ++stats_.rows_in;
    ++stats_.rows_out;
    return StageState::kAdvanced;
}

void CollectionScanStage::onClose() noexcept {
    // Nothing to release: table_ ownership is shared, not owned exclusively.
}

}  // namespace querylume
