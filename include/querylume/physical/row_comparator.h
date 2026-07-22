#pragma once

#include <cstddef>

#include "querylume/data/row.h"

namespace querylume {

enum class SortDirection {
    kAscending,
    kDescending,
};

// Single shared ordering abstraction used by BOTH SortStage and TopKStage so
// the two stages cannot accidentally implement different tie-breaking or
// null-placement semantics.
//
// Ordering rules (see docs/decisions.md):
//   - Non-null values order via querylume::compare() on the sort column.
//   - Nulls sort AFTER non-null values in ascending order, and BEFORE
//     non-null values in descending order.
//   - The row's original ordinal is the final tie-breaker (ascending by
//     ordinal), guaranteeing deterministic output regardless of direction.
class RowComparator {
public:
    RowComparator(std::size_t column_index, SortDirection direction)
        : column_index_(column_index), direction_(direction) {}

    // Returns true if `a` should be ordered strictly before `b`.
    bool operator()(const Row& a, const Row& b) const;

private:
    std::size_t column_index_;
    SortDirection direction_;
};

}  // namespace querylume
