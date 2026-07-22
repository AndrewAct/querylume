# Architecture

QueryLume turns a declarative JSON pipeline into executed rows through six
stages:

```
JSON data + JSON pipeline
        |
      Parser        (querylume/parser)
        |
      Binder         (querylume/binder)
        |
   Logical Plan       (querylume/logical)
        |
  Rule Optimizer      (querylume/optimizer)
        |
 Physical Planner     (querylume/physical/physical_planner.h)
        |
  PlanStage Tree      (querylume/physical)
        |
      Results  +  EXPLAIN / Runtime Statistics   (querylume/explain)
```

## Parser vs. binder

The parser (`parsePipeline`, `src/parser/pipeline_parser.cpp`) only
validates JSON *shape*: is `$match`'s value an object, is `$project`'s
value an array of strings, is `$limit`'s value an integer. It recognizes
stage names (`$match`/`$project`/`$sort`/`$limit`) and comparison-operator
*tokens* (`"$eq"`, `"$gte"`, ...) as opaque strings -- it does not check
whether a field exists or an operator is supported. Its output,
`ParsedPipeline`, is a tree of "unresolved" structs
(`UnresolvedFieldExpression` holds only a field name string).

The binder (`bindPipeline`, `src/binder/binder.cpp`) resolves every
unresolved reference against a `Schema`, in pipeline order, threading the
*currently visible* schema through the stages: a `$sort` after a `$project`
resolves against the projected schema, not the original input schema. This
is where every semantic check lives: field existence (`kUnknownField`),
operator support (`kUnsupportedOperator`), sort direction validity
(`kInvalidSortDirection`), and limit non-negativity (`kInvalidLimit`). The
binder's output is a `LogicalPlanNode` tree whose field references are
`BoundFieldExpression`s holding a resolved column index -- no stage ever
looks up a field by name again after this point.

This split exists so that "what does the pipeline literally say" and "is
what it says valid against this data" are two separable questions, each
tested independently (`tests/unit/pipeline_parser_test.cpp` vs.
`tests/unit/binder_test.cpp`).

## Logical vs. physical plan

The logical plan (`querylume/logical/`) describes *what* the query
computes: `LogicalScan`, `LogicalFilter`, `LogicalProject`, `LogicalSort`,
`LogicalLimit`, `LogicalTopK`. These are pure data + a `kind()` tag; they
have no `open()`/`getNext()` and cannot be executed. The physical plan
(`querylume/physical/`) describes *how*: a tree of `PlanStage` objects that
actually pull and produce `Row`s. `buildPhysicalPlan()`
(`src/physical/physical_planner.cpp`) is the one place that knows the 1:1
mapping between the two (`LogicalFilter` -> `FilterStage`, `LogicalTopK` ->
`TopKStage`, etc.), converted via a `switch` on `LogicalNodeKind` --
deliberately not a general visitor framework, since there are exactly six
closed node kinds in this MVP.

Keeping these separate is what lets the optimizer rewrite the *logical*
tree (Sort+Limit -> TopK) without touching any execution code, and lets
EXPLAIN show three different views of the same query (as-parsed,
as-optimized, as-physically-planned) from three different data structures
rather than three modes of one.

## Volcano iterator execution

Every `PlanStage` implements the pull-based (Volcano) interface:

```cpp
virtual void open();
virtual StageState getNext(Row& output);
virtual void close() noexcept;
```

A parent stage's `getNext()` calls its child's `getNext()` as many times as
it needs (once, for `FilterStage`/`ProjectStage`/`LimitStage`; until
exhaustion, for `SortStage`/`TopKStage`, which must see every row before
producing their first output row). Rows flow one at a time; there is no
batching. See `docs/decisions.md` ("Why execution is tuple-at-a-time") for
the tradeoff this implies.

**Lifecycle contract** (enforced centrally by `PlanStageBase`, in
`include/querylume/physical/plan_stage_base.h` /
`src/physical/plan_stage_base.cpp`, so no individual stage re-implements
it):

- `open()` must be called exactly once before the first `getNext()`.
  Calling it again -- before or after `close()` -- throws
  `QueryLumeError(kInvalidStageLifecycle)`. **Reopening after close is not
  supported**; construct a new stage tree instead.
- `getNext()` before `open()`, or after `close()`, throws the same error.
- Once `getNext()` returns `kEof`, every subsequent call also returns
  `kEof` (idempotent EOF) without re-invoking the stage's own logic.
- `close()` is idempotent and `noexcept`; it is safe to call multiple
  times, and safe to call without ever reaching EOF.
- Empty input is well-defined: `open()` succeeds, the first `getNext()`
  returns `kEof`.

`PlanStageBase` also owns all per-stage timing (wrapping each `onOpen()`/
`onGetNext()` call in a `steady_clock` measurement) and call-count
statistics, so concrete stages (`FilterStage`, `SortStage`, ...) only
implement `onOpen()`/`onGetNext()`/`onClose()` and never touch lifecycle
state directly.

## Ownership model

- Every stage owns its child via `std::unique_ptr<PlanStage>` (never a raw
  owning pointer). `PlanStage::child()` (a defaulted virtual returning
  `nullptr` for leaves) exposes a *non-owning* raw pointer purely for
  EXPLAIN's tree-walking serialization -- it is never used to drive
  execution.
- Every logical node owns its child the same way
  (`std::unique_ptr<LogicalPlanNode>`), plus a mutable accessor
  (`mutableChild()`) used by the optimizer rule to detach and reattach
  subtrees during rewriting, and by the physical planner to recurse.
- `LogicalScan` and `CollectionScanStage` hold the input `Table` via
  `std::shared_ptr<const Table>` rather than uniquely: the same loaded data
  can back two independently-bound logical plans (used by the
  optimized-vs-unoptimized integration test, which binds the same pipeline
  twice and only optimizes one copy) without copying rows.
- Expression trees (`BoundFieldExpression`, `LiteralExpression`,
  `ComparisonExpression`, `ConjunctionExpression`) are owned via
  `std::unique_ptr<Expression>`, transferred by move from `LogicalFilter`
  into `FilterStage` at physical-planning time
  (`LogicalFilter::takePredicate()`).

## Blocking vs. streaming operators

| Stage | Streaming? | Notes |
|---|---|---|
| `CollectionScanStage` | streaming | O(1) additional space |
| `FilterStage` | streaming | O(1) additional space |
| `ProjectStage` | streaming | O(1) additional space |
| `LimitStage` | streaming | stops pulling once `k` rows emitted |
| `SortStage` | **blocking** | materializes all input in `onOpen()` before emitting |
| `TopKStage` | **blocking** | consumes all input in `onOpen()`, keeps a bounded heap |

"Blocking" here means the stage's `onOpen()` fully drains its child before
returning, rather than lazily pulling per `getNext()` call -- a necessary
consequence of needing global information (the full row set) before any
output row's position is known.

## Sort vs. TopK

Both stages produce identical output for the equivalent
`Sort(field, direction) -> Limit(k)` composition, and both share **one**
ordering primitive, `RowComparator`
(`include/querylume/physical/row_comparator.h`), specifically so they
cannot silently diverge on null placement or tie-breaking:

- Non-null values order via `compare()` on the sort column.
- Nulls sort **after** non-null values ascending, **before** non-null
  values descending.
- The row's original ordinal is the final tie-breaker, guaranteeing
  deterministic output regardless of direction or duplicate keys.

`SortStage` is `O(n log n)` time / `O(n)` space (materialize + `std::sort`).
`TopKStage` is `O(n log k)` time / `O(k)` space (a bounded binary heap via
`std::push_heap`/`std::pop_heap`, evicting the current worst-of-k whenever a
better row arrives, then a final `O(k log k)` sort of the retained rows to
produce output order). `SortLimitToTopKRule` (the optimizer's one rule)
rewrites `Limit(k) -> Sort(...) -> Child` into `TopK(..., k) -> Child`
wherever that adjacency occurs in the plan (not only at the root), trading
`O(n log n)` for `O(n log k)` when `k` is much smaller than `n`. See
`benchmarks/sort_vs_topk_benchmark.cpp` for measurements.

## Statistics collection

Two distinct kinds of statistics exist:

- **Per-stage** (`StageStats`, one instance per `PlanStage`): `rows_in`,
  `rows_out`, `open_calls`, `get_next_calls`, `execution_time_micros`,
  `peak_memory_bytes`. Collected automatically by `PlanStageBase` (calls
  and timing) plus each concrete stage (row counts, and for `SortStage`/
  `TopKStage`, an approximate memory figure -- see
  `docs/decisions.md`). `execution_time_micros` is **inclusive of every
  descendant's time** (see `docs/decisions.md`, "Why stage timing is
  inclusive") -- do not sum it across a tree.
- **Global** (`GlobalStats`, one instance per query): `documentsExamined`
  (rows produced by the leaf `CollectionScanStage`), `documentsReturned`
  (final row count), `totalExecutionTimeMicros` (the root stage's inclusive
  time), `optimizerRulesApplied` (count of rewrite-trace entries with
  `applied == true`). Computed once, in `explainAnalyzePipeline`
  (`src/explain/query_facade.cpp`), after execution.

EXPLAIN builds the full physical stage tree but never calls
`open()`/`getNext()` on it, so its stats would all be zero -- the
`toJson(PlanStage&, include_stats)` serializer takes `include_stats=false`
for EXPLAIN and `true` for EXPLAIN ANALYZE, omitting the `stats` key
entirely in the former case rather than showing misleading zeros.
