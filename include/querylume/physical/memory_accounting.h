#pragma once

#include <cstdint>
#include <vector>

#include "querylume/data/row.h"

namespace querylume {

// Approximate, deterministic memory accounting shared by the two blocking
// stages (SortStage, TopKStage). "Approximate" because it counts the
// logical byte footprint of each Value (fixed struct size plus string
// content length) rather than actual allocator bytes, which vary by
// platform/allocator and would make peak_memory_bytes non-reproducible.
// See docs/decisions.md for the full rationale.
std::uint64_t approximateRowBytes(const Row& row);
std::uint64_t approximateRowsBytes(const std::vector<Row>& rows);

}  // namespace querylume
