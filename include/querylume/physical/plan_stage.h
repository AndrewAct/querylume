#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "querylume/data/row.h"

namespace querylume {

enum class StageState {
    kAdvanced,
    kEof,
};

// Per-stage runtime counters. `execution_time_micros` is INCLUSIVE of every
// descendant's time, because getNext() recursively pulls from children
// before doing its own work: summing execution_time_micros across a tree
// double- (or n-) counts shared subtree time. Use it only to inspect a
// single stage's own share by subtracting its immediate children's times,
// never by summing the whole tree. See docs/architecture.md "Statistics".
struct StageStats {
    std::uint64_t rows_in = 0;
    std::uint64_t rows_out = 0;
    std::uint64_t open_calls = 0;
    std::uint64_t get_next_calls = 0;
    std::uint64_t execution_time_micros = 0;
    std::uint64_t peak_memory_bytes = 0;
};

// Volcano-style pull iterator. Lifecycle contract:
//   - open() must be called exactly once before the first getNext() call.
//     Calling open() a second time without an intervening close() throws
//     QueryLumeError(kInvalidStageLifecycle).
//   - getNext() may be called repeatedly. Once it returns kEof, every
//     subsequent call also returns kEof (idempotent EOF) without touching
//     `output`.
//   - getNext() before open(), or after close(), throws
//     QueryLumeError(kInvalidStageLifecycle).
//   - close() releases resources and is safe to call multiple times
//     (idempotent) and with noexcept. It must not throw.
//   - Reopening after close() is NOT supported: open() after close() throws
//     QueryLumeError(kInvalidStageLifecycle). Construct a new stage tree
//     instead of reusing one.
//   - Empty input is well-defined: open() succeeds, the first getNext()
//     returns kEof.
//   - Every stage owns its child exclusively via std::unique_ptr<PlanStage>.
class PlanStage {
public:
    virtual ~PlanStage() = default;

    virtual void open() = 0;
    virtual StageState getNext(Row& output) = 0;
    virtual void close() noexcept = 0;

    virtual const StageStats& stats() const noexcept = 0;
    virtual std::string stageName() const = 0;

    // Non-owning read-only access to this stage's child, for tree
    // introspection (EXPLAIN serialization) only -- never for execution.
    // Returns nullptr for leaf stages (e.g. CollectionScanStage). Ownership
    // remains with the parent's unique_ptr<PlanStage>.
    virtual PlanStage* child() const noexcept { return nullptr; }
};

}  // namespace querylume
