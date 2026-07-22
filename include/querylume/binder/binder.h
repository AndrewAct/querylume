#pragma once

#include <memory>

#include "querylume/data/table.h"
#include "querylume/logical/logical_plan_node.h"
#include "querylume/parser/parsed_pipeline.h"

namespace querylume {

// Resolves a ParsedPipeline against `table`'s schema into a logical plan
// tree. Performs every semantic check the parser deferred: field
// existence, operator support, sort-direction validity, limit
// non-negativity (see parsed_pipeline.h for the parser/binder split).
// Field resolution threads the *currently visible* schema through the
// pipeline in stage order, so a $sort after a $project resolves against the
// projected schema, not the original input schema.
std::unique_ptr<LogicalPlanNode> bindPipeline(std::shared_ptr<const Table> table,
                                              const ParsedPipeline& pipeline);

}  // namespace querylume
