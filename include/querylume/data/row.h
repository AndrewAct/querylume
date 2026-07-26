#pragma once

#include <cstdint>
#include <vector>

#include "querylume/data/value.h"

namespace querylume {

// A single row of scalar values plus the ordinal it had in the original
// input document array. The ordinal survives every stage (filter, project,
// sort, top-k) and is used as the final deterministic tie-breaker whenever
// two rows compare equal on the requested sort key.
struct Row {
    std::uint64_t ordinal = 0;
    std::vector<Value> values;
};

}  // namespace querylume
