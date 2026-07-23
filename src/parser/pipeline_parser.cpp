#include "querylume/parser/pipeline_parser.h"

#include <limits>

#include "querylume/common/error.h"

namespace querylume {

namespace {

Value jsonToLiteral(const nlohmann::json& value) {
    switch (value.type()) {
        case nlohmann::json::value_t::null:
            return nullptr;
        case nlohmann::json::value_t::boolean:
            return value.get<bool>();
        case nlohmann::json::value_t::number_integer:
            return value.get<std::int64_t>();
        case nlohmann::json::value_t::number_unsigned: {
            const auto unsigned_value = value.get<std::uint64_t>();
            if (unsigned_value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                throw QueryLumeError(ErrorCode::kUnsupportedValueType,
                                     "comparison integer literal is outside the int64 range");
            }
            return static_cast<std::int64_t>(unsigned_value);
        }
        case nlohmann::json::value_t::number_float:
            return value.get<double>();
        case nlohmann::json::value_t::string:
            return value.get<std::string>();
        default:
            throw QueryLumeError(ErrorCode::kPipelineSyntaxError,
                                 "comparison literal must be a scalar (null, bool, number, or string)");
    }
}

[[noreturn]] void syntaxError(const std::string& message) {
    throw QueryLumeError(ErrorCode::kPipelineSyntaxError, message);
}

std::int64_t parseInt64(const nlohmann::json& value, const std::string& context) {
    if (value.type() == nlohmann::json::value_t::number_integer) {
        return value.get<std::int64_t>();
    }
    if (value.type() == nlohmann::json::value_t::number_unsigned) {
        const auto unsigned_value = value.get<std::uint64_t>();
        if (unsigned_value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            return static_cast<std::int64_t>(unsigned_value);
        }
    }
    syntaxError(context + " must fit in a signed 64-bit integer");
}

ParsedMatchStage parseMatch(const nlohmann::json& value) {
    if (!value.is_object()) {
        syntaxError("$match value must be a JSON object mapping field names to predicates");
    }
    ParsedMatchStage stage;
    for (auto field_it = value.begin(); field_it != value.end(); ++field_it) {
        const nlohmann::json& operand = field_it.value();
        if (!operand.is_object() || operand.size() != 1) {
            syntaxError("$match predicate for field '" + field_it.key() +
                        "' must be a single-key object like {\"$gte\": <literal>}");
        }
        auto op_it = operand.begin();
        ParsedComparison comparison;
        comparison.field.field_name = field_it.key();
        comparison.op_token = op_it.key();
        comparison.literal = jsonToLiteral(op_it.value());
        stage.predicates.push_back(std::move(comparison));
    }
    return stage;
}

ParsedProjectStage parseProject(const nlohmann::json& value) {
    if (!value.is_array()) {
        syntaxError("$project value must be a JSON array of field names");
    }
    ParsedProjectStage stage;
    for (const auto& field : value) {
        if (!field.is_string()) {
            syntaxError("$project field names must be strings");
        }
        stage.fields.push_back(UnresolvedFieldExpression{field.get<std::string>()});
    }
    return stage;
}

ParsedSortStage parseSort(const nlohmann::json& value) {
    if (!value.is_object() || value.size() != 1) {
        syntaxError("$sort value must be a single-key object like {\"field\": 1}");
    }
    auto it = value.begin();
    if (!it.value().is_number_integer()) {
        syntaxError("$sort direction for field '" + it.key() + "' must be an integer (1 or -1)");
    }
    ParsedSortStage stage;
    stage.field.field_name = it.key();
    stage.direction_raw = parseInt64(it.value(), "$sort direction for field '" + it.key() + "'");
    return stage;
}

ParsedLimitStage parseLimit(const nlohmann::json& value) {
    if (!value.is_number_integer()) {
        syntaxError("$limit value must be an integer");
    }
    ParsedLimitStage stage;
    stage.count_raw = parseInt64(value, "$limit value");
    return stage;
}

}  // namespace

ParsedPipeline parsePipeline(const nlohmann::json& pipeline_json) {
    if (!pipeline_json.is_array()) {
        syntaxError("pipeline must be a JSON array of stage objects");
    }

    ParsedPipeline pipeline;
    for (const auto& stage_json : pipeline_json) {
        if (!stage_json.is_object() || stage_json.size() != 1) {
            syntaxError("each pipeline stage must be a single-key object, e.g. {\"$match\": {...}}");
        }
        auto it = stage_json.begin();
        const std::string& stage_name = it.key();
        const nlohmann::json& value = it.value();

        ParsedStage stage;
        if (stage_name == "$match") {
            stage.kind = ParsedStageKind::kMatch;
            stage.match = parseMatch(value);
        } else if (stage_name == "$project") {
            stage.kind = ParsedStageKind::kProject;
            stage.project = parseProject(value);
        } else if (stage_name == "$sort") {
            stage.kind = ParsedStageKind::kSort;
            stage.sort = parseSort(value);
        } else if (stage_name == "$limit") {
            stage.kind = ParsedStageKind::kLimit;
            stage.limit = parseLimit(value);
        } else {
            throw QueryLumeError(ErrorCode::kUnknownStage, "unknown pipeline stage: " + stage_name);
        }
        pipeline.stages.push_back(std::move(stage));
    }
    return pipeline;
}

}  // namespace querylume
