#pragma once

#include "querylume/data/schema.h"
#include "querylume/logical/logical_plan_node.h"

namespace querylume {

// Recovers the output Schema a logical plan node produces, by walking down
// to the nearest LogicalProject (or the LogicalScan, if there is none).
// Used to label result rows with field names for JSON output.
Schema outputSchemaOf(const LogicalPlanNode& node);

}  // namespace querylume
