#include "querylume/data/json_loader.h"

#include <fstream>
#include <sstream>
#include <unordered_map>

#include "querylume/common/error.h"

namespace querylume {

namespace {

Value convertScalar(const nlohmann::json& value, const std::string& field) {
    switch (value.type()) {
        case nlohmann::json::value_t::null:
            return nullptr;
        case nlohmann::json::value_t::boolean:
            return value.get<bool>();
        case nlohmann::json::value_t::number_integer:
        case nlohmann::json::value_t::number_unsigned:
            return value.get<int64_t>();
        case nlohmann::json::value_t::number_float:
            return value.get<double>();
        case nlohmann::json::value_t::string:
            return value.get<std::string>();
        case nlohmann::json::value_t::object:
        case nlohmann::json::value_t::array:
            throw QueryLumeError(ErrorCode::kInvalidDataShape,
                                 "field '" + field + "' has an unsupported nested object/array value");
        default:
            throw QueryLumeError(ErrorCode::kUnsupportedValueType,
                                 "field '" + field + "' has an unsupported JSON value type");
    }
}

}  // namespace

Table loadTableFromJson(const nlohmann::json& document) {
    if (!document.is_array()) {
        throw QueryLumeError(ErrorCode::kInvalidDataShape, "input data must be a JSON array of documents");
    }

    std::vector<std::string> field_order;
    std::unordered_map<std::string, std::size_t> field_index;

    for (const auto& doc : document) {
        if (!doc.is_object()) {
            throw QueryLumeError(ErrorCode::kInvalidDataShape, "each input document must be a JSON object");
        }
        for (auto it = doc.begin(); it != doc.end(); ++it) {
            if (field_index.find(it.key()) == field_index.end()) {
                field_index.emplace(it.key(), field_order.size());
                field_order.push_back(it.key());
            }
        }
    }

    Table table;
    table.schema = Schema(field_order);
    table.rows.reserve(document.size());

    std::uint64_t ordinal = 0;
    for (const auto& doc : document) {
        Row row;
        row.ordinal = ordinal++;
        row.values.assign(field_order.size(), Value(nullptr));
        for (auto it = doc.begin(); it != doc.end(); ++it) {
            const std::size_t index = field_index.at(it.key());
            row.values[index] = convertScalar(it.value(), it.key());
        }
        table.rows.push_back(std::move(row));
    }

    return table;
}

Table loadTableFromJsonText(const std::string& text) {
    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(text);
    } catch (const nlohmann::json::parse_error& e) {
        throw QueryLumeError(ErrorCode::kJsonParseError, std::string("failed to parse JSON: ") + e.what());
    }
    return loadTableFromJson(parsed);
}

Table loadTableFromJsonFile(const std::string& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw QueryLumeError(ErrorCode::kJsonParseError, "failed to open data file: " + path);
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return loadTableFromJsonText(buffer.str());
}

}  // namespace querylume
