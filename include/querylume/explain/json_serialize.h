#pragma once

#include <nlohmann/json.hpp>

#include "querylume/data/row.h"
#include "querylume/data/schema.h"
#include "querylume/data/value.h"
#include "querylume/execution/global_stats.h"
#include "querylume/logical/logical_plan_node.h"
#include "querylume/optimizer/optimization_context.h"
#include "querylume/parser/parsed_pipeline.h"
#include "querylume/physical/plan_stage.h"

namespace querylume {

// All JSON serialization for EXPLAIN / EXPLAIN ANALYZE / query results lives
// here so the parser/logical/physical modules stay free of a JSON
// dependency beyond what they already need for their own data.
nlohmann::json toJson(const Value& value);
nlohmann::json toJson(const Row& row, const Schema& schema);
nlohmann::json toJson(const ParsedPipeline& pipeline);
nlohmann::json toJson(const LogicalPlanNode& node);
nlohmann::json toJson(const std::vector<RewriteTraceEntry>& trace);

// include_stats=true embeds each stage's StageStats (rowsIn, rowsOut,
// openCalls, getNextCalls, executionTimeMicros, peakMemoryBytes) alongside
// its shape. Pass false for EXPLAIN (which must not execute the plan, so
// stats would all be zero and are omitted instead).
nlohmann::json toJson(const PlanStage& stage, bool include_stats);
nlohmann::json toJson(const GlobalStats& stats);

}  // namespace querylume
