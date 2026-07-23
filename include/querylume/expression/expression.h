#pragma once

#include <functional>
#include <string>
#include <utility>
#include <variant>

#include "querylume/data/row.h"
#include "querylume/data/value.h"

namespace querylume {

// An expression result is either a borrowed reference to an existing Value
// (field and literal expressions) or a newly owned Value (comparison/AND
// results). This small wrapper keeps scalar expressions composable without
// copying string cells on every predicate evaluation.
//
// The wrapper never outlives evaluate()'s input Row during execution:
// ComparisonExpression and ConjunctionExpression consume child results
// immediately, and FilterStage only inspects the final boolean.
class EvaluationResult {
public:
    explicit EvaluationResult(std::reference_wrapper<const Value> borrowed) : storage_(borrowed) {}
    explicit EvaluationResult(Value owned) : storage_(std::move(owned)) {}

    const Value& value() const noexcept {
        if (const auto* borrowed = std::get_if<std::reference_wrapper<const Value>>(&storage_)) {
            return borrowed->get();
        }
        return std::get<Value>(storage_);
    }

private:
    std::variant<std::reference_wrapper<const Value>, Value> storage_;
};

// Base class for bound expression-tree nodes. All expressions are built by
// the binder against a resolved Schema; there is no re-resolution of field
// names during row execution (see BoundFieldExpression).
class Expression {
public:
    virtual ~Expression() = default;

    virtual EvaluationResult evaluate(const Row& row) const = 0;

    // Stable, human-readable description used by EXPLAIN serialization.
    virtual std::string describe() const = 0;
};

}  // namespace querylume
