# Design Decisions

This document records the "why" behind choices that aren't obvious from the
code alone. See `docs/architecture.md` for how the pieces fit together and
`README.md` for usage.

## Why C++17

The spec settles on C++17 as the implementation language: it is the oldest
standard with the guarantees the design leans on --
`std::variant` (the `Value` type), structured bindings, `if constexpr`
(unused directly but available), and mandatory copy elision for the
value-returning factory functions used throughout (`bindPipeline`,
`buildPhysicalPlan`, `explainPipeline`). Nothing in QueryLume needs C++20
concepts, ranges, or coroutines, and pinning to C++17 keeps the toolchain
requirement low (any reasonably recent Clang/GCC/MSVC) without losing any
capability the design actually uses.

## Why execution is tuple-at-a-time (Volcano/iterator model)

`PlanStage::getNext()` produces one `Row` per call. The alternative --
vectorized/batch execution -- would improve cache behavior and amortize
virtual-call overhead, but it also requires columnar batch buffers, batch
size tuning, and null-handling per batch, none of which the MVP's scope
calls for. Tuple-at-a-time keeps every stage's contract small (open / get
one row / close), keeps memory bounded to O(1) for streaming stages
(Scan, Filter, Project, Limit), and makes the blocking-vs-streaming
distinction (see architecture.md) easy to reason about. Given the MVP's
explicit non-goals (no cost-based optimizer, no vectorized execution, no
concurrency), the simplicity of one-row-at-a-time pull execution outweighs
its throughput cost at this scale.

## Why the MVP is in-memory and single-source

There is exactly one input: a JSON array of flat documents, loaded fully
into a `Table` before any stage runs. No storage engine, no persistence, no
WAL, no incremental/streaming ingestion. This mirrors the explicit
non-goals list (no storage engine, no persistence, no WAL, no MVCC). It also
simplifies the data model: `Table::rows` is a `std::vector<Row>`, and
`CollectionScanStage` just walks it. If QueryLume ever needed multiple
sources, that would imply joins -- also explicitly out of scope.

## Why DuckDB and Arrow are excluded from Phase 1

The product definition is "an embeddable C++ query execution kernel" --
the point of this project is to *build* a query engine (parser, binder,
logical plan, optimizer, physical planner, Volcano execution, EXPLAIN), not
to wrap an existing one. Using DuckDB or Arrow for execution would replace
the thing being demonstrated with a dependency. They are excluded from
Phase 1 by design, not because they're unsuitable -- a later phase that
needs a real storage/columnar layer might reasonably reach for Arrow, but
that is out of scope here.

## Why only one optimizer rule

`SortLimitToTopKRule` is the only rule because it's the only rewrite the
spec asks for, and it's a clean, self-contained demonstration of the
rule-based optimizer *shape* (an `OptimizerRule` interface, an
`OptimizationContext` for trace recording, a driver that runs the
registered rules in order) without needing a cost model, statistics, or
rule ordering/fixpoint logic that a second rule would start to require.
Adding filter pushdown or projection pruning was explicitly called out as
out of scope; the rule list structure (`Optimizer::rules_`) exists so a
future rule doesn't require changing the driver, not as an invitation to
add one now.

## Why schema-backed rows instead of per-row hash maps

`Row` is a `std::vector<Value>` positional tuple; field names are resolved
to column indices once, in the binder (`BoundFieldExpression::column_index_`,
`ProjectStage::column_indices_`, `RowComparator::column_index_`). The
alternative -- each row as an `unordered_map<string, Value>` -- would let
the parser skip the binding step entirely, but it pays a hash + string
comparison on every field access, for every row, for the lifetime of the
query. Given the spec's explicit requirement ("avoid field-name lookup in
the hot path"), positional rows resolved once at bind time are the correct
tradeoff: binding cost is paid once per query, not once per row.

## Why stage timing is inclusive of children

`StageStats::execution_time_micros` is accumulated by `PlanStageBase`
around each `onOpen()`/`onGetNext()` call, and since a stage's own
`onGetNext()` recursively calls its child's `getNext()`, the recorded time
necessarily includes every descendant's time. Making timing *exclusive*
(a stage's own work only) would require subtracting children's
already-elapsed time at every level, which in turn requires either passing
extra state through every stage or a second post-hoc tree walk correlating
parent/child timestamps -- real complexity for a number that, in the MVP,
is only ever displayed, never used to drive a decision (no cost-based
optimizer consumes it). Callers must not sum `execution_time_micros`
across the tree: the root stage's value already IS the whole tree's time.
Per-stage "self time" can be approximated by subtracting the immediate
child's inclusive time, documented here rather than computed automatically.

## Value comparison semantics

Documented at the point of implementation (`include/querylume/data/value.h`)
and repeated here for visibility:

- `int64_t` and `double` compare numerically. Integer/integer comparisons stay
  exact, and mixed comparisons use range and fractional-part checks instead of
  blindly converting the integer to `double`; this preserves distinctions above
  2^53 and avoids undefined out-of-range floating-point-to-integer casts.
- Strings compare lexicographically (byte-wise `std::string::operator<`).
- Booleans support only `==`/`!=`; ordered comparison between two booleans
  throws `QueryLumeError(kIncompatibleTypes)`.
- `null == null` is `true`; `null` compared for equality against any
  non-null value is `false`.
- Any ordered comparison (`<`, `<=`, `>`, `>=`) touching `null` evaluates to
  `false` for `$match` predicates (see `ComparisonExpression::evaluate`),
  which is distinct from how `SortStage`/`TopKStage` place nulls (see
  below) -- filtering and ordering are different operations with different
  null semantics, both explicitly required by the spec.
- Comparisons between incompatible non-numeric types (e.g. string vs.
  number, bool vs. string) throw `QueryLumeError(kIncompatibleTypes)`.
- For sort/top-k ordering: nulls sort after non-null values in ascending
  order and before non-null values in descending order; the original row
  ordinal is always the final tie-breaker. This is implemented once, in
  `RowComparator`, and shared by `SortStage` and `TopKStage` so the two
  stages cannot silently diverge.

The binder does **not** statically type-check literal/field compatibility
beyond field existence, because the data model has no static per-column
type (a column's values may legitimately mix `int64_t` and `double` across
rows, or contain `null`). The spec's "literal and field types are
compatible where they can be determined" is satisfied by the fact that,
for this schema-less-per-cell model, that determination genuinely cannot be
made until a specific row's value is known -- so it's made at evaluation
time by `compare()`/`valuesEqual()`, which throw `kIncompatibleTypes` on a
genuine mismatch.

## Why memory accounting is approximate but deterministic

`approximateRowBytes()` (`querylume/physical/memory_accounting.h`) counts
`sizeof(Row)` plus each string value's `.size()` (content length), not
`.capacity()` or actual allocator bytes. Real allocator bytes depend on the
platform's malloc implementation and small-string-optimization thresholds,
which differ across standard library implementations and are therefore not
reproducible across environments. Content-length-based accounting gives
the same number for the same input on any conforming implementation, which
matters more for this MVP than absolute accuracy: `peak_memory_bytes` is a
diagnostic signal in EXPLAIN ANALYZE output, not a memory-limit enforcement
mechanism (there is no memory limiting in this MVP).

## Why physical planning consumes the logical plan

`buildPhysicalPlan(std::unique_ptr<LogicalPlanNode>)` deliberately takes
ownership of the logical root. A filter owns its expression with
`std::unique_ptr`, and physical planning moves that expression into the
corresponding `FilterStage`; recursively, it also moves ownership of every
child node. The function therefore consumes the logical tree rather than
quietly leaving a half-moved tree behind a mutable reference.

EXPLAIN captures the pre-optimization and post-optimization logical JSON
snapshots before calling the physical planner. This ordering is explicit in
`explain::planQuery`: after ownership is transferred, there is no logical tree
left to inspect accidentally. An alternative would be a deep `clone()` on
every expression and plan node, but that adds allocation and maintenance cost
without serving an MVP requirement.

## Why schema order is lexicographic

JSON object members are semantically unordered, and `nlohmann::json` uses an
ordered map for objects by default. QueryLume therefore constructs the union
schema in lexicographic field-name order rather than promising the visual order
in which keys happened to appear in an input document. This gives identical
column indices, plans, and EXPLAIN output for semantically identical JSON whose
members were written in different orders.

## Why predicate evaluation can borrow values

A bound field expression often refers directly to a cell already stored in the
input row. Returning a complete `Value` from every expression would copy that
cell, including allocating for strings, once per predicate evaluation. The
`EvaluationResult` type can instead hold either a borrowed `const Value&` or an
owned temporary. Bound fields and literals borrow; comparisons and conjunctions
own their computed boolean result. The result object keeps this lifetime choice
explicit while avoiding copies in the common field-access path.

## Why there is no per-row debug logging

Logging inside `getNext()` or expression evaluation would add formatting,
branching, synchronization, and potentially I/O to QueryLume's hottest path.
It can also produce unbounded output for a large input. The MVP instead exposes
structured observability through EXPLAIN, EXPLAIN ANALYZE, optimizer traces, and
per-stage counters/timing. Comments explain ownership and lifecycle rules in
the implementation. If operational logging is added later, it should be an
optional injected interface, disabled by default, with coarse query/lifecycle
events rather than one message per row.
