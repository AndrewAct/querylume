#pragma once

#include <string>
#include <vector>

namespace querylume {

struct RewriteTraceEntry {
    std::string rule;
    bool applied;
};

// Mutable state threaded through a single optimization pass. Rules record
// their own trace entries here so EXPLAIN can show exactly which rules ran
// and whether each one fired.
struct OptimizationContext {
    std::vector<RewriteTraceEntry> trace;
};

}  // namespace querylume
