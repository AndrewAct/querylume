#pragma once

#include <memory>

#include "querylume/expression/expression.h"
#include "querylume/physical/plan_stage_base.h"

namespace querylume {

// Pulls rows from its child, evaluates the bound predicate, and emits only
// matching rows. O(n) time, O(1) additional space beyond the child.
class FilterStage final : public PlanStageBase {
public:
    FilterStage(std::unique_ptr<PlanStage> child, std::unique_ptr<Expression> predicate)
        : child_(std::move(child)), predicate_(std::move(predicate)) {}

    std::string stageName() const override { return "FilterStage"; }
    PlanStage* child() const noexcept override { return child_.get(); }

protected:
    void onOpen() override;
    StageState onGetNext(Row& output) override;
    void onClose() noexcept override;

private:
    std::unique_ptr<PlanStage> child_;
    std::unique_ptr<Expression> predicate_;
};

}  // namespace querylume
