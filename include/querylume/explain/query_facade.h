#pragma once

#include <memory>
#include <nlohmann/json.hpp>

#include "querylume/data/row.h"
#include "querylume/data/schema.h"
#include "querylume/data/table.h"

namespace querylume {

struct QueryResult {
    std::vector<Row> rows;
    Schema output_schema;
};

// `querylume run`: parses, binds, optimizes, physically plans, and
// executes the pipeline, returning the final rows and their schema.
QueryResult runPipeline(const std::shared_ptr<const Table>& table, const nlohmann::json& pipeline_json);

// `querylume explain`: builds every plan stage (parsed pipeline, logical
// plan, optimized logical plan, rewrite trace, physical plan shape) WITHOUT
// executing the physical plan -- stats are omitted since they would all be
// zero.
nlohmann::json explainPipeline(const std::shared_ptr<const Table>& table,
                               const nlohmann::json& pipeline_json);

// `querylume explain-analyze`: same as explainPipeline, but also executes
// the plan and includes per-stage runtime stats, global execution
// statistics, and (if `include_results`) the query result rows.
nlohmann::json explainAnalyzePipeline(const std::shared_ptr<const Table>& table,
                                      const nlohmann::json& pipeline_json, bool include_results);

}  // namespace querylume
