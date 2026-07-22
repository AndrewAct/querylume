#pragma once

#include <cstdint>

namespace querylume {

// Query-wide execution statistics, distinct from per-stage StageStats.
struct GlobalStats {
    std::uint64_t documents_examined = 0;
    std::uint64_t documents_returned = 0;
    std::uint64_t total_execution_time_micros = 0;
    std::uint64_t optimizer_rules_applied = 0;
};

}  // namespace querylume
