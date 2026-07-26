#pragma once

#include <memory>

#include "querylume/data/table.h"
#include "querylume/physical/plan_stage_base.h"

namespace querylume {

// Leaf stage: iterates the in-memory Table in original row order. The Table
// is held by shared_ptr<const Table> (not owned uniquely) so the same loaded
// data can back multiple independently-executed plans, e.g. the optimized
// and unoptimized plans compared in integration tests, without copying rows.
class CollectionScanStage final : public PlanStageBase {
public:
    explicit CollectionScanStage(std::shared_ptr<const Table> table);

    const Schema& outputSchema() const noexcept { return table_->schema; }
    std::string stageName() const override { return "CollectionScanStage"; }

protected:
    void onOpen() override;
    StageState onGetNext(Row& output) override;
    void onClose() noexcept override;

private:
    std::shared_ptr<const Table> table_;
    std::size_t next_index_ = 0;
};

}  // namespace querylume
