#pragma once

#include <nlohmann/json.hpp>

#include "querylume/parser/parsed_pipeline.h"

namespace querylume {

// Parses a JSON pipeline (array of single-key stage objects, e.g.
// [{"$match": {...}}, {"$project": [...]}, ...]) into a ParsedPipeline.
// Only validates JSON shape and recognizes stage/operator-key names; see
// parsed_pipeline.h for the parser/binder split. Throws QueryLumeError with
// kPipelineSyntaxError, kUnknownStage, or kUnsupportedValueType as
// appropriate.
ParsedPipeline parsePipeline(const nlohmann::json& pipeline_json);

}  // namespace querylume
