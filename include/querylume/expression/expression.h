#pragma once

#include <string>

#include "querylume/data/row.h"
#include "querylume/data/value.h"

namespace querylume {

// Base class for bound expression-tree nodes. All expressions are built by
// the binder against a resolved Schema; there is no re-resolution of field
// names during row execution (see BoundFieldExpression).
class Expression {
public:
    virtual ~Expression() = default;

    virtual Value evaluate(const Row& row) const = 0;

    // Stable, human-readable description used by EXPLAIN serialization.
    virtual std::string describe() const = 0;
};

}  // namespace querylume
