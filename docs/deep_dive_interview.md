# QueryLume — Deep Dive Interview Document

Study document for defending QueryLume in a technical interview. It follows
the Project Deep Dive Interview Protocol: every claim below is tied to a
file, a test, or a number actually produced by this repository (see the
`Tests and validation` / evidence lines in each section). `docs/architecture.md`
and `docs/decisions.md` are the primary sources of truth for *how* and *why*
the system is built this way; this document is the interview-facing
synthesis — organized by "what will get asked," not by source layout.

---

## 0. Project Startup Dossier

**Goal:** an embeddable, single-process C++17 library that takes a
declarative JSON pipeline (Mongo-aggregation-flavored: `$match`/`$project`/
`$sort`/`$limit`) plus a flat JSON document array, and executes it —
producing result rows, an `EXPLAIN` plan, or an `EXPLAIN ANALYZE` with
per-stage runtime statistics.

**Users:** a hypothetical embedding application (via `QueryLume::Core` +
CMake `find_package`), and directly via the bundled `querylume_cli` binary
(`run` / `explain` / `explain-analyze` subcommands).

**Success metrics (Phase 1 MVP, as actually verified, not aspirational):**
- Correctness: 75/75 tests passing (`ctest --test-dir build`, verified
  2026-07-24 — see §6 for the breakdown; this is a live number from running
  the suite, not carried over from an earlier session's memory).
- Optimizer correctness: optimized and unoptimized plans proven to produce
  *identical* output (`tests/integration/optimized_vs_unoptimized_test.cpp`).
- Complexity: `TopKStage` measurably faster than `SortStage`+`LimitStage`
  at large `n`, small `k` (§5, benchmark evidence).
- Static analysis clean: clang-format and clang-tidy both clean (per prior
  session; re-verify before citing in an interview if it's been a while).
- Sanitizers: UBSan passes clean; ASan is wired up but hangs in this
  particular sandboxed dev environment during shadow-memory init — a
  documented environment limitation, not a code defect (`README.md`, "Known
  environment limitation").

**Architecture map:**

```
JSON data + JSON pipeline
        |
      Parser        (shape validation only)      -> src/parser/pipeline_parser.cpp
        |
      Binder         (schema resolution)          -> src/binder/binder.cpp
        |
   Logical Plan       (pure data, 6 node kinds)   -> src/logical/
        |
  Rule Optimizer      (1 rule: Sort+Limit->TopK)  -> src/optimizer/
        |
 Physical Planner     (1:1 logical->physical)     -> src/physical/physical_planner.cpp
        |
  PlanStage Tree      (Volcano pull iterators)    -> src/physical/*_stage.cpp
        |
      Results + EXPLAIN / EXPLAIN ANALYZE         -> src/explain/
```

**Tech stack table:**

| Technology | Responsibility | Why chosen | Alternative rejected | Tradeoff |
|---|---|---|---|---|
| C++17 | Implementation language | `std::variant`, structured bindings, guaranteed copy elision — nothing in the design needs C++20 concepts/ranges/coroutines | C++20 | Lower toolchain requirement, no functionality lost for this scope |
| CMake ≥ 3.20 + FetchContent | Build + dependency management | Standard for C++ libraries meant to be embedded via `add_subdirectory`; no external package manager assumed | Conan / vcpkg | FetchContent pins exact commit/tag so builds are reproducible without a separate package-manager install step, at the cost of a longer first configure |
| nlohmann/json | JSON parse/serialize | De facto standard single-header-ish JSON lib, ergonomic API, appears in QueryLume's own public API surface | RapidJSON, simdjson | Slower than simdjson, but the MVP has no throughput requirement on JSON parsing itself and nlohmann's API is far less boilerplate |
| CLI11 | CLI argument parsing | Header-only, modern C++ idioms, good subcommand support (`run`/`explain`/`explain-analyze`) | hand-rolled `argv` parsing, getopt | Small dependency cost for correctness/UX that would otherwise be hand-maintained |
| GoogleTest | Unit/integration testing | Industry standard, `gtest_discover_tests` integrates cleanly with CTest | Catch2 | No strong reason either way; GTest chosen for familiarity/tooling maturity |
| Google Benchmark | Microbenchmarking | Handles iteration-count calibration, `DoNotOptimize`, statistical stability — a hand-rolled `chrono` loop would be unreliable | hand-rolled timing loop | Adds a dependency just for benchmarks (gated behind `QUERYLUME_BUILD_BENCHMARKS`, off by default when embedded) |
| Volcano/iterator execution model | Query execution strategy | Textbook pull-based model (Graefe 1994); keeps each stage's contract to `open/getNext/close`, bounds memory for streaming stages to O(1) | Vectorized/batch execution | Batch execution would improve cache behavior and amortize virtual-call overhead, but needs columnar buffers, batch-size tuning, per-batch null handling — none required by this MVP's scope |

**Phase map:**
- **Phase 1 (done, this repo):** parser → binder → logical plan → one
  optimizer rule → physical planner → Volcano execution → EXPLAIN/EXPLAIN
  ANALYZE → CLI → tests → benchmarks → UBSan → install/export packaging.
- **Phase 1.5 (the "hardening" commit, `504bcb3`):** after the MVP worked,
  a second pass fixed real correctness/lifecycle issues found by review —
  see §7 (Behavioral Story Seeds) for what this is worth in an interview.
- **Phase 2 (explicitly out of scope, not started):** cost-based
  optimization, filter pushdown, projection pruning, `$group`/aggregation,
  joins, indexes, persistence, concurrency — see `README.md` "Future
  roadmap."

**Quality gates:** clang-format, clang-tidy, CTest (75 tests across unit /
execution / optimizer / integration), UBSan build, Google Benchmark for
the one performance-sensitive claim (Sort vs TopK), GitHub Actions CI
(`.github/workflows/ci.yml`) including install/embedding smoke tests.

**Interview surface — the claims most likely to get attacked:**
1. "Implemented a rule-based query optimizer" → single rule, no cost model, no fixpoint loop (§4).
2. "Volcano-style execution engine" → tuple-at-a-time, not vectorized; what does that cost? (§3)
3. "TopK optimization is O(n log k)" → prove it, show the benchmark, explain the crossover (§4/§5).
4. "Modular CMake, embeddable library" → prove it doesn't force tests/CLI on an embedder (§6).
5. "Correct under duplicate/retry/failure conditions" → this is an in-memory, single-shot batch engine with no persistence or retries; the honest answer is *why* those failure modes don't apply here, not a fabricated answer (§3, Known gaps).

---

## 1. Parser / Binder split

**Problem:** a single "parse and validate" pass conflates two independent
questions — is the JSON *shaped* like a pipeline, and does what it says
make *semantic* sense against this specific data's schema. Conflating them
makes both harder to test and means a syntactically valid but
semantically invalid pipeline (e.g., referencing a nonexistent field)
can't be distinguished from a malformed one.

**Current behavior:** `parsePipeline` (`src/parser/pipeline_parser.cpp`)
checks JSON shape only — is `$match`'s value an object, is `$project`'s
value a string array, is `$limit`'s value an integer — and treats field
names and operator tokens (`"$eq"`, `"$gte"`, ...) as opaque strings. Its
output (`ParsedPipeline`) holds `UnresolvedFieldExpression`s (field name
strings only, no column index).

`bindPipeline` (`src/binder/binder.cpp`) resolves every unresolved
reference against a `Schema`, threading the *currently visible* schema
through pipeline stages in order — so `$sort` after `$project` resolves
against the *projected* schema, not the original input schema. This is
where every semantic error is raised: `kUnknownField`,
`kUnsupportedOperator`, `kInvalidSortDirection`, projection-field
uniqueness, `kInvalidLimit`. Output is a `LogicalPlanNode` tree with
`BoundFieldExpression`s holding a resolved column index — no stage ever
does a field-name lookup again after this point.

**Architecture:** two independent, sequential passes; parser has zero
knowledge of `Schema`.

**Primary data/control flow:**
`JSON pipeline → parsePipeline() → ParsedPipeline (unresolved) → bindPipeline(schema) → LogicalPlanNode tree (bound)`

**Key decisions:** field names resolved to column indices *once*, at bind
time — not re-looked-up per row (see §4/decisions.md, "avoid field-name
lookup in the hot path").

**Alternatives rejected:** a single combined parse+validate pass (rejected
— conflates two independently-testable concerns); per-row
`unordered_map<string, Value>` rows that let the parser skip binding
entirely (rejected — pays a hash + string comparison per field access per
row for the query's lifetime; see §4).

**Correctness invariants:** the currently-visible schema threaded through
binding must reflect prior `$project`/`$sort` stages, not the original
input schema; a `BoundFieldExpression`'s column index is only valid
against the schema state at the point it was bound.

**Failure modes:** unknown field → `kUnknownField`; unsupported operator
token → `kUnsupportedOperator`; invalid sort direction (not `1`/`-1`) →
`kInvalidSortDirection`; duplicate `$project` field → rejected; negative
`$limit` → `kInvalidLimit`. All are raised at bind time, before any
execution — a malformed query never partially executes.

**Scaling limits:** binding is O(pipeline stages × fields referenced), a
one-time cost per query — not per row. This is the whole point of the
design (see §4).

**Tests and validation:** `tests/unit/pipeline_parser_test.cpp` (shape
validation, independent of schema) vs. `tests/unit/binder_test.cpp`
(semantic resolution and every documented error category) — the split is
directly reflected in the test file split.

**Observability:** parser and binder failures surface as
`QueryLumeError` with a specific error code (see `include/querylume/common/error.h`),
not a generic exception.

**Known gaps:** no static type checking of literal/field compatibility
beyond existence — the data model has no per-column static type (a column
may legitimately mix `int64_t`/`double`/`null` across rows), so type
compatibility is checked at row-evaluation time instead (`docs/decisions.md`,
"Value comparison semantics").

**Interview challenge questions:**
- Why not let the parser also resolve fields, since it's already walking the tree?
- What happens if `$sort` references a field that a later `$project` would have dropped?
- Why column-index resolution instead of a hash map keyed by field name — what's actually saved, and where's the evidence?

---

## 2. Logical plan vs. physical plan

**Problem:** if "what a query computes" and "how it's executed" are the
same data structure, an optimizer rewrite has to already understand
execution mechanics, and EXPLAIN can't show a plan *shape* without
building (and potentially running) real executable objects.

**Current behavior:** the logical plan (`querylume/logical/`) is six pure
data node kinds — `LogicalScan`, `LogicalFilter`, `LogicalProject`,
`LogicalSort`, `LogicalLimit`, `LogicalTopK` — with a `kind()` tag and no
`open()`/`getNext()`; they cannot be executed. The physical plan
(`querylume/physical/`) is a tree of `PlanStage` objects that actually pull
rows. `buildPhysicalPlan()` (`src/physical/physical_planner.cpp`) is the
single place that knows the 1:1 mapping (`LogicalFilter → FilterStage`,
`LogicalTopK → TopKStage`, ...), via a `switch` on `LogicalNodeKind` —
deliberately not a general visitor framework, since there are exactly six
closed node kinds in this MVP (YAGNI applied deliberately, not an
oversight).

**Architecture:** `buildPhysicalPlan(std::unique_ptr<LogicalPlanNode>)`
*consumes* the logical tree — this is a real ownership boundary, not
incidental API shape: a filter's expression is moved from the logical
node into the physical `FilterStage`, so retaining the old logical tree
afterward would leave a partially-moved object graph. EXPLAIN therefore
captures pre- and post-optimization logical JSON snapshots *before*
calling the physical planner — there's no tree left to inspect after.

**Primary data/control flow:**
`LogicalPlanNode tree (post-optimizer) --consumed by--> buildPhysicalPlan() --produces--> PlanStage tree (independently owned)`

**Key decisions:** separating logical/physical is what lets the optimizer
rewrite `Sort+Limit → TopK` purely on the logical tree without touching
any execution code, and lets EXPLAIN show three distinct views (as-parsed,
as-optimized, as-physically-planned) from three distinct data structures.

**Alternatives rejected:** deep `clone()` on every expression/node so the
logical tree could be retained non-destructively — rejected as added
allocation/maintenance cost with no MVP requirement driving it
(`docs/decisions.md`, "Why physical planning consumes the logical plan").

**Correctness invariants:** physical planning must fully consume
(move-from) the logical tree exactly once; no code path may read the
logical tree after `buildPhysicalPlan()` returns.

**Failure modes:** N/A for malformed input at this stage — by the time a
tree reaches the physical planner it has already passed binding; the
planner's job is a mechanical 1:1 lowering, not validation.

**Scaling limits:** the `switch`-based lowering is O(number of logical
nodes) and does not scale as a *design* to a large open set of node kinds
— that's an explicit, acknowledged tradeoff for exactly six kinds, not a
hidden limitation.

**Tests and validation:** `tests/integration/optimized_vs_unoptimized_test.cpp`
binds the same pipeline twice, optimizes only one copy, and asserts
*identical* output — this is what proves the logical/physical split and
the optimizer rewrite don't change query semantics.

**Observability:** `EXPLAIN` output shows `logicalPlan`,
`optimizedLogicalPlan`, and `physicalPlan` as three separate JSON
sub-objects in one response.

**Known gaps:** none specific to this layer beyond the closed-node-kind
scaling note above.

**Interview challenge questions:**
- Why a `switch` instead of a visitor pattern or double dispatch — what would force you to switch approaches?
- What would break if `buildPhysicalPlan` took a `const&` instead of consuming ownership?
- How does `LogicalScan`/`CollectionScanStage` sharing the same `Table` via `shared_ptr<const Table>` interact with this ownership model? (Answer: it's the one deliberate exception — shared read-only data, not owned/moved state — see `docs/architecture.md` "Ownership model.")

---

## 3. Volcano iterator execution & lifecycle contract

**Problem:** a pull-based execution engine needs a small, uniform
contract every operator implements, and that contract's edge cases
(double-open, get-next-after-EOF, exception during open, double-close)
are exactly where correctness bugs live if each operator reimplements
them independently.

**Current behavior:** every `PlanStage` implements
`open() / getNext(Row&) / close() noexcept`. A parent calls its child's
`getNext()` as needed — once per call for streaming stages
(`FilterStage`/`ProjectStage`/`LimitStage`), until exhaustion for blocking
stages (`SortStage`/`TopKStage`, which must see every row before producing
their first output). Rows flow one at a time; there is no batching.

**Architecture:** the lifecycle contract is enforced **centrally** by
`PlanStageBase` (`include/querylume/physical/plan_stage_base.h` /
`src/physical/plan_stage_base.cpp`) — no individual stage reimplements it.
Concrete stages only implement `onOpen()`/`onGetNext()`/`onClose()`;
`PlanStageBase` wraps these with lifecycle-state checks and per-call
timing (`steady_clock`).

**Primary data/control flow:**
`open()` (once, before first getNext) → repeated `getNext()` until `kEof`
→ `close()` (idempotent, `noexcept`, safe even without reaching EOF).
Top-level execution is wrapped in `PlanStageExecutionGuard`, whose
destructor calls `close()` — so an exception thrown from predicate
evaluation, sorting, or result collection still triggers the same cleanup
path as a normal run.

**Key decisions:**
- `open()` twice, or `getNext()` before `open()`/after `close()`, throws
  `QueryLumeError(kInvalidStageLifecycle)`.
- Reopening after `close()` is **not** supported — construct a new stage
  tree instead. This is a deliberate scope cut, not an oversight.
- Once `getNext()` returns `kEof`, every subsequent call also returns
  `kEof` without re-invoking the stage's own logic (idempotent EOF).
- Empty input is well-defined: `open()` succeeds, first `getNext()`
  returns `kEof` immediately.
- If `open()` fails after partially opening a child, `PlanStageBase`
  immediately closes that partial subtree before rethrowing — no leaked
  half-open children on the exception path.
- Blocking stages release materialized row buffers *during* `close()`,
  not deferred to destruction.

**Alternatives rejected:** vectorized/batch execution (better cache
behavior, amortized virtual-call cost) — rejected because it requires
columnar batch buffers, batch-size tuning, and per-batch null handling,
none of which this MVP's scope calls for; tuple-at-a-time keeps every
stage's contract to three methods and keeps streaming-stage memory at
O(1) (`docs/decisions.md`).

**Correctness invariants:** exactly the six bullet points above under Key
decisions — each one is independently testable and tested (see below).

**Failure modes:** double-open, get-next-after-close, get-next-before-open
→ all throw `kInvalidStageLifecycle` rather than undefined behavior;
exception during `open()` on a multi-child tree → partial subtree is
closed before the exception propagates, so no resource/stat leak.

**Scaling limits:** tuple-at-a-time means per-row virtual-dispatch
overhead (`getNext()` is virtual) on every stage boundary — this is the
explicit cost being paid for contract simplicity; a vectorized engine
would amortize this over a batch at the cost of much more implementation
complexity. This is the single most defensible "what would you do at
100x scale" answer: switch to columnar batches, but only once profiling
shows virtual-dispatch/row overhead actually dominates.

**Tests and validation:** `tests/execution/plan_stage_test.cpp` covers
the lifecycle contract directly (double-open, get-next ordering,
idempotent EOF, close-without-EOF, exception-during-open cleanup — this
is exactly the kind of test suite the "hardening" commit `504bcb3` added,
see §7).

**Observability:** `PlanStageBase` also collects `StageStats` (rows_in,
rows_out, open_calls, get_next_calls, execution_time_micros,
peak_memory_bytes) automatically for every stage — see §5.

**Known gaps:** no concurrency (single-threaded only, by explicit
non-goal); no reopening after close (by explicit design choice, not a
missing feature — reconstructing a stage tree is cheap in this MVP's
scope).

**Interview challenge questions:**
- Walk through exactly what happens if a `SortStage`'s comparator throws mid-`onOpen()`. (Answer: partial materialization is abandoned, `PlanStageBase` closes the partially-opened child, exception propagates — verified this doesn't leak because `close()` is idempotent/noexcept and the guard always runs.)
- Why `noexcept` on `close()` specifically? What would break if it could throw?
- Duplicate input / retry / restart — how does this system's answer differ from a distributed system's, and why is that an honest answer rather than a gap? (It's in-memory, single-shot, single-process — there's no restart/retry concept because there's no persisted state to recover; this is a scope boundary, not an unaddressed failure mode.)

---

## 4. Rule-based optimizer: `SortLimitToTopKRule` (Sort vs. TopK)

**Problem:** a common query shape — sort by one field, keep only the top
`k` — is correct but wasteful if executed literally: full `O(n log n)`
sort followed by truncation, when a bounded `O(n log k)` structure would
produce the identical result.

**Current behavior:** `SortLimitToTopKRule` (the *only* optimizer rule)
rewrites `Limit(k) → Sort(field, direction) → Child` into
`TopK(field, direction, k) → Child` wherever that adjacency occurs in the
plan — not only at the plan root (`tests/optimizer/sort_limit_to_topk_rule_test.cpp::RewriteDeepInChainNotJustAtRoot`
proves this). Both `SortStage` and `TopKStage` produce byte-identical
output for the equivalent composition because they share **one** ordering
primitive, `RowComparator` (`include/querylume/physical/row_comparator.h`)
— this is deliberate: two independent comparator implementations could
silently diverge on null placement or tie-breaking, and sharing one
primitive makes that divergence structurally impossible rather than
just tested-against.

**Architecture:** `Optimizer` (`src/optimizer/optimizer.cpp`) is a small
driver that runs registered `OptimizerRule`s in order against the logical
tree, recording an `OptimizationContext` rewrite trace (rule name,
whether it applied) — this trace is what `EXPLAIN`'s `rewrites` array
surfaces.

**Primary data/control flow:**
`LogicalPlanNode tree (as-bound) → Optimizer::optimize() → walks tree, applies SortLimitToTopKRule wherever Limit→Sort adjacency is found → optimized LogicalPlanNode tree + rewrite trace`

**Key decisions:**
- Ordering semantics defined once and shared: non-null values compare via
  `compare()`; nulls sort **after** non-null in ascending order, **before**
  non-null in descending order; the row's original ordinal is the final
  tie-breaker (deterministic output regardless of duplicate keys).
- `SortStage`: `O(n log n)` time, `O(n)` space (materialize all input,
  `std::sort`).
- `TopKStage`: `O(n log k)` time, `O(k)` space (bounded binary heap via
  `std::push_heap`/`std::pop_heap`, evicting the current worst-of-k when a
  better row arrives; final `O(k log k)` sort of the retained `k` rows to
  produce output order).

**Alternatives rejected:** a second rule (e.g. filter pushdown or
projection pruning) — explicitly deferred; adding one would require a
cost model, rule ordering, or fixpoint iteration logic that a single rule
doesn't need. The `Optimizer::rules_` list structure exists so a *future*
rule wouldn't require changing the driver — it is not an invitation to
add one now without a corresponding design discussion (`docs/decisions.md`,
"Why only one optimizer rule"). This is a good answer to "why does your
optimizer only have one rule" — it's a scope decision with a stated
reason, not a limitation you ran out of time for.

**Correctness invariants:** for any input, `Sort(f,d)→Limit(k)` and
`TopK(f,d,k)` must produce identical output rows in identical order — this
is a hard invariant, not a best-effort approximation, and it's what makes
the rewrite safe to apply automatically rather than opt-in.

**Failure modes:** N/A for runtime failure — the rule is purely a
tree-shape rewrite over already-bound, already-validated logical nodes.
The risk is a *correctness* bug (comparator divergence), which is why the
shared `RowComparator` exists as a structural guard, not just a tested
convention.

**Scaling limits:** the crossover point where `TopK` wins is when `k ≪ n`
(the whole point of trading `log n` for `log k`); at `k` approaching `n`,
`TopK`'s extra bookkeeping (heap push/pop plus a final `k log k` sort)
would erode the advantage — this MVP doesn't compute a cost-based
crossover threshold, it applies the rewrite unconditionally whenever the
pattern matches, since for this MVP's scope any `k < n` case is a
correctness-preserving, monotonically-no-worse rewrite in complexity
terms (heap operations are still cheaper asymptotically for any `k ≤ n`).

**Tests and validation:**
`tests/optimizer/sort_limit_to_topk_rule_test.cpp` — adjacent case
rewrites, non-adjacent does *not* rewrite, `Limit` without `Sort` does not
rewrite, rewrite fires below the plan root (not just at root).
`tests/integration/optimized_vs_unoptimized_test.cpp` — binds the same
pipeline twice, optimizes one copy only, asserts identical rows/order
between the two executions — this is the actual proof that the
optimization is semantics-preserving, not just "it compiles and looks
right."

**Observability:** `benchmarks/sort_vs_topk_benchmark.cpp` — the
quantitative evidence for the rewrite's value. Re-run locally
(`Debug` build, this machine, 2026-07-24):

```
BM_SortPlusLimit/100000/10   ~119 ms   (847K items/s)
BM_TopK/100000/10            ~13.1 ms  (7.62M items/s)   → ~9x faster
BM_SortPlusLimit/100000/100  ~131 ms   (791K items/s)
BM_TopK/100000/100           ~13.7 ms  (7.31M items/s)   → ~9.6x faster
```

Caveat for interview use: this was a `Debug` build (`***WARNING*** Library
was built as DEBUG`), not the `Release` config the README recommends for
real numbers — cite the *ratio* and the *asymptotic reasoning*
(`O(n log n)` vs `O(n log k)`), not the absolute millisecond figures, and
re-run in Release before quoting a specific multiplier as a headline
number.

**Known gaps:** no cost-based decision about *whether* to apply the
rewrite — it's unconditional whenever the pattern matches, which is
correct here because the rewrite is complexity-monotonic (never worse),
but a more general optimizer would need real cost estimation before
applying a rewrite that could regress.

**Interview challenge questions:**
- Prove the rewrite is safe — where does that proof live? (Answer: shared `RowComparator` + the optimized-vs-unoptimized integration test, not an informal argument.)
- Why is the rewrite unconditional instead of cost-gated on `k` vs `n`? What real system would need a cost gate here, and why doesn't this one?
- What happens if two `Sort→Limit` pairs are stacked (`Sort→Limit→Sort→Limit`)? Does the rule fire on both, and is that still correct? (Worth tracing through the rule's match logic before answering live.)

---

## 5. EXPLAIN / EXPLAIN ANALYZE / statistics collection

**Problem:** debugging a query plan and profiling its execution are
different needs — one is static (what will run), one is dynamic (what did
run, how long did each part take) — and conflating them either bloats
`EXPLAIN`'s output with misleading zeros or forces every `EXPLAIN` call to
pay real execution cost.

**Current behavior:** `EXPLAIN` builds the full physical stage tree but
**never calls** `open()`/`getNext()` on it — so its stats would all be
zero if included; `toJson(PlanStage&, include_stats)` takes
`include_stats=false` for `EXPLAIN` (omits the `stats` key entirely) and
`true` for `EXPLAIN ANALYZE` (executes, then serializes real counters).

**Architecture:** two distinct statistics types —
`StageStats` (per-`PlanStage`: rows_in, rows_out, open_calls,
get_next_calls, execution_time_micros, peak_memory_bytes), collected
automatically by `PlanStageBase` plus each concrete stage; and
`GlobalStats` (per-query: documentsExamined, documentsReturned,
totalExecutionTimeMicros, optimizerRulesApplied), computed once in
`explainAnalyzePipeline` (`src/explain/query_facade.cpp`) after execution
completes.

**Primary data/control flow:**
`EXPLAIN`: `parse → bind → optimize → physical-plan → serialize (no execution)`.
`EXPLAIN ANALYZE`: same, plus `open→drain→close`, then serialize with
stats populated.

**Key decisions:** `execution_time_micros` is **inclusive of every
descendant's time** — a stage's `onGetNext()` recursively calls its
child's `getNext()`, so the timer necessarily wraps that. Making it
exclusive (self-time only) would require subtracting children's elapsed
time at every level (extra state threaded through every stage, or a
second post-hoc tree walk) — real complexity for a number that, in this
MVP, is only ever *displayed*, never consumed by a decision (no
cost-based optimizer reads it). **Callers must not sum this value across
a tree** — the root stage's value already *is* the whole tree's time.

**Alternatives rejected:** computing exclusive per-stage self-time
automatically — rejected as unjustified complexity for a display-only
number; documented as "subtract the immediate child's inclusive time
yourself" instead (`docs/decisions.md`, "Why stage timing is inclusive").

**Correctness invariants:** `EXPLAIN` must never show non-zero stats (that
would misrepresent a plan that didn't run); `EXPLAIN ANALYZE`'s
`documentsReturned` must equal the actual result row count when
`--with-result` is set.

**Failure modes:** N/A specific to this layer — it's a read-only view over
already-validated structures, or a real execution wrapped in the same
lifecycle guarantees as `run` (§3).

**Scaling limits:** the inclusive-timing tradeoff (above) is itself a
scaling-of-effort decision: not worth solving until something *consumes*
exclusive self-time programmatically.

**Tests and validation:** `tests/integration/explain_golden_test.cpp` — a
golden-file test pinning EXPLAIN's JSON shape, plus
`ExplainDoesNotExecutePhysicalPlan` (asserts zero-execution for plain
`EXPLAIN`) and `ExplainAnalyzeGoldenTest.ContainsNonZeroStageCountersAndMatchesResultSize`
(asserts real counters for `EXPLAIN ANALYZE`).

**Observability:** this *is* the observability layer for the whole
project — no per-row debug logging exists by design (`docs/decisions.md`,
"Why there is no per-row debug logging": would add formatting/branching/
I/O to the hottest path and produce unbounded output; structured
EXPLAIN/stats/optimizer-trace output is the deliberate substitute).

**Known gaps:** `peak_memory_bytes` is an approximate, deterministic,
content-length-based estimate (`sizeof(Row)` + each string's `.size()`),
not actual allocator bytes — chosen because real allocator bytes aren't
reproducible across platforms/stdlib implementations, and this number is
a diagnostic signal, not a memory-limiting mechanism (there is no memory
limiting in this MVP).

**Interview challenge questions:**
- Why is stage timing inclusive instead of exclusive — what's the actual cost of making it exclusive, concretely?
- If you needed a real memory limit (not just a diagnostic estimate), what would have to change in `memory_accounting.h`?
- Why does `EXPLAIN` capture logical-plan snapshots *before* calling the physical planner rather than after? (Ties back to §2 — the physical planner consumes/moves the logical tree.)

---

## 6. Build system: modular CMake + FetchContent

**Problem:** a library meant to be both a standalone developer project
(with tests, CLI, benchmarks) *and* an embeddable dependency
(`add_subdirectory()` from a parent project) needs a build that doesn't
force the embedding case to pay for the developer-only case.

**Current behavior:** one `CMakeLists.txt` per logical component
(`src/`, `apps/querylume_cli/`, `tests/`, `benchmarks/`), tied together by
the root `CMakeLists.txt` via `add_subdirectory()`. `option()` flags
(`QUERYLUME_BUILD_CLI`, `QUERYLUME_BUILD_TESTS`, `QUERYLUME_BUILD_BENCHMARKS`)
default to `ON` only when QueryLume is the *top-level* project
(`CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR` — `QUERYLUME_TOP_LEVEL_DEFAULT`,
`CMakeLists.txt:21-25`) and default to `OFF` when embedded, so a parent
project doesn't unexpectedly build QueryLume's tests/CLI/benchmarks.

**Architecture:**
- `querylume_core` (`src/CMakeLists.txt`) — the actual library, exported
  under the stable alias `QueryLume::Core` (`add_library(QueryLume::Core ALIAS querylume_core)`)
  so consumers link against a name that doesn't change even if the
  underlying target is renamed internally.
- `querylume_warnings` — an `INTERFACE` target bundling
  `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion`,
  applied `PRIVATE` to `querylume_core` (via `$<BUILD_INTERFACE:...>` so
  it never leaks into a consumer's build flags) — warnings-as-development-policy
  kept as a reusable target instead of copy-pasted compiler flags.
- Dependency fetching centralized in `cmake/Dependencies.cmake`, gated per
  optional component (GoogleTest only fetched if `QUERYLUME_BUILD_TESTS`,
  Google Benchmark only if `QUERYLUME_BUILD_BENCHMARKS`) — avoids paying
  clone/build cost for dependencies a given configuration doesn't need.
- Install/export rules (`CMakePackageConfigHelpers`,
  `QueryLumeConfig.cmake.in`) so an installed consumer can
  `find_package(QueryLume 0.1 REQUIRED)` and link `QueryLume::Core` —
  added specifically in the hardening pass (`504bcb3`, "add installable
  CMake package and QueryLume::Core target").

**Primary data/control flow:**
`cmake -S . -B build` → root `CMakeLists.txt` sets options/policies →
`include(cmake/Dependencies.cmake)` (FetchContent-declares + conditionally
makes-available nlohmann_json/CLI11/googletest/googlebenchmark) →
`add_subdirectory(src)` (always) → conditionally `add_subdirectory(apps/querylume_cli)`,
`add_subdirectory(tests)`, `add_subdirectory(benchmarks)` → package/export
rules for `cmake --install`.

**Key decisions:** target-based dependency declaration throughout
(`target_link_libraries`/`target_include_directories` with
`PUBLIC`/`PRIVATE`/`INTERFACE` visibility) rather than directory-global
`include_directories()` — this is what makes `QueryLume::Core`'s public
include path (`$<BUILD_INTERFACE:...include>` /
`$<INSTALL_INTERFACE:...>`) precise and non-leaking into unrelated
targets.

**Alternatives rejected:** Conan/vcpkg for dependency management —
FetchContent chosen instead so a fresh clone + `cmake -S . -B build` works
with zero separate package-manager bootstrap step, at the cost of
CMake-driving-the-clone being slower on first configure than a
pre-resolved package-manager cache.

**Correctness invariants:** an embedding parent project must never be
forced to build QueryLume's tests/CLI/benchmarks or their dependencies
(GoogleTest/Benchmark/CLI11) unless it explicitly opts in.

**Failure modes:** mixing sanitized and non-sanitized translation units in
one binary is unsupported and can crash at startup — this is why
`QUERYLUME_ENABLE_ASAN`/`QUERYLUME_ENABLE_UBSAN` apply
`add_compile_options`/`add_link_options` **globally** (root
`CMakeLists.txt:41-53`) rather than via a linkable interface target, so
every translation unit — including FetchContent'd dependencies — is built
consistently.

**Scaling limits:** N/A in the runtime sense; the scaling concern here is
build-time and organizational — one `CMakeLists.txt` per component keeps
the graph legible as components are added, versus one monolithic root
file that would become unreadable.

**Tests and validation:** CI (`.github/workflows/ci.yml`) runs the normal
build/test, plus dedicated install and embedding smoke-test build
directories in this repo (`build-install-smoke/`, `build-embedding-smoke/`)
that verify `find_package`/`add_subdirectory` consumption actually works,
not just that the top-level build works.

**Observability:** `CMAKE_EXPORT_COMPILE_COMMANDS ON` (root
`CMakeLists.txt:15`) for IDE/clang-tidy tooling integration.

**Known gaps:** none significant — this is a fairly complete, idiomatic
"Modern CMake" setup for a small embeddable library. If pressed for a
gap: no `CPack` packaging, no prebuilt binary distribution, no
cross-compilation toolchain files provided.

**Interview challenge questions:**
- Why `ALIAS` targets (`QueryLume::Core`) instead of just letting consumers link `querylume_core` directly?
- Why does enabling ASan/UBSan use global `add_compile_options` instead of an interface target like `querylume_warnings`? (Answer: sanitizer flags must apply to *every* TU including fetched dependencies, or you get a mixed-instrumentation crash at startup — an interface target only applied to targets that explicitly link it wouldn't reach FetchContent'd deps.)
- What would you have to change if a consumer needed a *different* major version of nlohmann/json than QueryLume pins? (README states: "version 3.11.3 or newer in the same major version must also be discoverable by the consuming CMake project" — this is a real, documented constraint, worth being able to explain rather than discovering live.)

---

## 7. Behavioral story seeds (grounded in actual commits)

Use these as raw material for BQ answers — each is tied to a real commit,
not a generic narrative.

- **Ownership / going beyond original scope:** the second commit,
  `504bcb3` ("fix: harden execution lifecycle and library integration"),
  came *after* the MVP already worked end-to-end. It added an RAII
  execution guard, fixed buffer release timing in `close()`, decoupled
  logical plans from physical sort definitions, made physical planning
  explicitly consume (rather than ambiguously reference) logical plans,
  fixed numeric boundary/JSON-integer handling, avoided unnecessary
  `Value` copies, added the installable CMake package, and expanded the
  test suite specifically for lifecycle/equivalence/CLI/boundary cases.
  None of that was strictly required for a working demo — it's the gap
  between "runs on the happy path" and "the ownership/lifecycle
  guarantees actually hold," found and closed as a deliberate second pass.
- **Tradeoff, stated and defensible:** tuple-at-a-time (Volcano) execution
  over vectorized/batch execution — chosen explicitly because the MVP's
  non-goals (no cost-based optimizer, no vectorized execution, no
  concurrency) mean the throughput cost of one-row-at-a-time pull
  execution is outweighed by keeping every stage's contract to three
  methods (§3). This is a good "what did you intentionally leave
  imperfect, and why was that acceptable" story.
- **Ambiguity resolved via decomposition:** the spec's non-goals list
  itself (no `$or`/`$not`, no joins, no aggregation, single sort field
  only) is the artifact of decomposing an open-ended "build a query
  engine" ask into a closed, testable Phase 1 — see `README.md` "Scope"
  and "Explicitly out of scope."
- **Failure/debugging, honestly scoped:** AddressSanitizer hangs
  indefinitely in this sandboxed dev environment during shadow-memory
  init, verified with a minimal standalone repro (`int main(){}`) outside
  the project entirely, confirming it's a platform/sandbox interaction and
  not a QueryLume defect — a good example of correctly bounding a
  debugging investigation (verify the environment before assuming the
  code is at fault) rather than chasing a phantom bug in your own code.
- **Learning / assumption that changed after implementation:** the
  original spec's tech-stack section said C++17, but its later
  "Definition of Done" section said C++20 — an internal spec
  inconsistency, resolved in favor of C++17 since nothing in the actual
  design needs C++20 features. Worth being able to explain *why* that
  resolution was correct (not just that a conflict existed).

---

## Appendix: current verified numbers (2026-07-24)

- `ctest --test-dir build`: **75/75 passed**, 0.60s total.
- `sort_vs_topk_benchmark` (Debug build, this machine): TopK ~9–9.6x
  faster than Sort+Limit at n=100,000 for k∈{10,100}. Re-run in Release
  before citing a specific multiplier in an interview — see §4 caveat.
- Git history: 5 commits (`42e47f8` first commit → `92085d2` MVP →
  `9ea660b` CI → `504bcb3` lifecycle hardening → `d493190` clang-tidy
  cleanup). Working tree clean at time of writing.
