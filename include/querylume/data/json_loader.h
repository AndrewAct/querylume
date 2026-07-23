#pragma once

#include <nlohmann/json.hpp>

#include "querylume/data/table.h"

namespace querylume {

// Loads a Table from a JSON array of flat documents.
//
// Schema inference rule: the schema is the union of all field names seen
// across all documents, ordered lexicographically. JSON objects are
// semantically unordered, so using an explicit canonical order avoids
// depending on a particular JSON parser's object-storage policy. Any
// document missing a field gets a null for that column.
// Nested objects and arrays as field values are rejected with
// QueryLumeError(kInvalidDataShape). The top-level JSON value must be an
// array of objects; anything else is QueryLumeError(kInvalidDataShape).
// Malformed JSON text raises QueryLumeError(kJsonParseError).
Table loadTableFromJson(const nlohmann::json& document);
Table loadTableFromJsonText(const std::string& text);
Table loadTableFromJsonFile(const std::string& path);

}  // namespace querylume
