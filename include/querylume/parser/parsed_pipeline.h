#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "querylume/data/value.h"

namespace querylume {

// Parser output: unresolved field references, raw operator tokens, and raw
// literal/scalar values. The parser only validates JSON *shape* (is this an
// object? an array of strings? an integer?) and recognizes stage names.
// Everything semantic -- does the field exist, is the operator supported,
// is the sort direction one of {1,-1}, is the limit non-negative -- is the
// binder's job (see querylume/binder). This mirrors the spec's explicit
// parser/binder split (section 8).
struct UnresolvedFieldExpression {
    std::string field_name;
};

struct ParsedComparison {
    UnresolvedFieldExpression field;
    std::string op_token;  // e.g. "$eq"; validated by the binder.
    Value literal;
};

// $match: implicit AND across all predicates. An empty predicate list means
// "match every row".
struct ParsedMatchStage {
    std::vector<ParsedComparison> predicates;
};

// $project: inclusion-only list of field names, in the order requested.
struct ParsedProjectStage {
    std::vector<UnresolvedFieldExpression> fields;
};

// $sort: exactly one field. `direction_raw` is whatever integer the pipeline
// JSON contained; the binder rejects anything other than 1 or -1.
struct ParsedSortStage {
    UnresolvedFieldExpression field;
    std::int64_t direction_raw = 1;
};

// $limit: `count_raw` is whatever integer the pipeline JSON contained; the
// binder rejects negative values.
struct ParsedLimitStage {
    std::int64_t count_raw = 0;
};

enum class ParsedStageKind {
    kMatch,
    kProject,
    kSort,
    kLimit,
};

struct ParsedStage {
    ParsedStageKind kind;
    ParsedMatchStage match;
    ParsedProjectStage project;
    ParsedSortStage sort;
    ParsedLimitStage limit;
};

struct ParsedPipeline {
    std::vector<ParsedStage> stages;
};

}  // namespace querylume
