# QueryLume

QueryLume is an embeddable, single-process C++17 query execution and
diagnostics engine. It parses a declarative JSON pipeline, binds field
references against an inferred schema, builds a logical plan, applies a
small rule-based optimizer, generates a physical `PlanStage` tree, executes
it with a Volcano-style pull iterator model, and can explain every step of
that process (`EXPLAIN`) or execute while reporting per-stage runtime
statistics (`EXPLAIN ANALYZE`).

```
JSON data + JSON pipeline
        |
      Parser -> Binder -> Logical Plan -> Rule Optimizer
        |             -> Physical Planner -> PlanStage Tree
        |
      Results  +  EXPLAIN / Runtime Statistics
```

See `docs/architecture.md` for how the pieces fit together and
`docs/decisions.md` for why they're built this way.

## Scope

This is the **Phase 1 MVP**. Supported pipeline stages: `$match`,
`$project`, `$sort`, `$limit`. Supported optimizer rewrite: adjacent
`Sort(field, direction) -> Limit(k)` becomes `TopK(field, direction, k)`.
Supported physical stages: `CollectionScanStage`, `FilterStage`,
`ProjectStage`, `SortStage`, `LimitStage`, `TopKStage`.

Explicitly out of scope (by design, not oversight): a SQL parser, any
storage engine/persistence/WAL/MVCC/transactions/indexes, joins,
`$group`/aggregation, distributed execution, any RPC/HTTP/server surface,
DuckDB/Arrow/Parquet/CSV execution, vector or full-text search,
cost-based optimization, filter pushdown, projection pruning, language
bindings, nested documents/arrays, a general expression language, and
concurrency. See the project spec for the full list.

## Supported pipeline syntax

```jsonc
[
  { "$match": {
      "sector": { "$eq": "technology" },
      "price":  { "$gte": 200 }
  }},
  { "$project": ["symbol", "price"] },
  { "$sort": { "price": -1 } },
  { "$limit": 10 }
]
```

- `$match`: implicit AND across predicates. Operators: `$eq`, `$ne`, `$lt`,
  `$lte`, `$gt`, `$gte`. No `$or`, `$not`, regex, nested predicates,
  field-to-field comparison, or computed expressions.
- `$project`: inclusion-only, in the order requested. Duplicate fields are
  rejected. No computed fields, renames, or exclusion projection.
- `$sort`: exactly one field, direction `1` (ascending) or `-1`
  (descending).
- `$limit`: a non-negative integer; `0` returns an empty result.

Input data is a flat JSON array of documents (see `examples/stocks.json`).
Nested objects and arrays as field values are rejected. A document missing
a field gets `null` for that column. The schema is the union of all field
names across all documents in canonical lexicographic order; JSON object
member order is not treated as query semantics.

## Build

Requires CMake >= 3.20, a Ninja-compatible generator, and a C++17 compiler.
Dependencies (nlohmann/json, CLI11, GoogleTest, Google Benchmark) are
fetched automatically via `FetchContent` and pinned to specific tags/hashes
in `cmake/Dependencies.cmake`.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build          # or: ./build/tests/querylume_tests
```

Release build:

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

When QueryLume is embedded with `add_subdirectory()`, the CLI, tests, and
benchmarks default to `OFF`; a parent project can opt into any of them with
`QUERYLUME_BUILD_CLI`, `QUERYLUME_BUILD_TESTS`, or
`QUERYLUME_BUILD_BENCHMARKS`. Link the library through its stable alias:

```cmake
add_subdirectory(path/to/querylume)
target_link_libraries(my_app PRIVATE QueryLume::Core)
```

QueryLume also provides install/export rules:

```sh
cmake --install build --prefix /desired/prefix
```

An installed consumer can use `find_package(QueryLume 0.1 REQUIRED)` and
`QueryLume::Core`. Because nlohmann/json appears in QueryLume's public API,
version 3.11.3 or newer in the same major version must also be discoverable
by the consuming CMake project.

Sanitizer builds (each is its own build directory; `-DQUERYLUME_BUILD_BENCHMARKS=OFF`
is optional but keeps the sanitizer build faster):

```sh
cmake -S . -B build-ubsan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQUERYLUME_ENABLE_UBSAN=ON
cmake --build build-ubsan
./build-ubsan/tests/querylume_tests

cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQUERYLUME_ENABLE_ASAN=ON
cmake --build build-asan
./build-asan/tests/querylume_tests
```

> **Known environment limitation:** in the sandboxed CLI environment this
> project was built in, AddressSanitizer's shadow-memory initialization
> (`FindDynamicShadowStart`) hangs indefinitely for *any* ASan-instrumented
> binary, including a trivial `int main() { return 0; }` -- verified with a
> minimal repro outside this project entirely. This is a platform/sandbox
> interaction, not a QueryLume defect. UBSan has no such issue and the full
> test suite passes clean under it. The ASan build target is fully wired up
> (`-DQUERYLUME_ENABLE_ASAN=ON`) and should be exercised in CI or on a
> non-sandboxed machine.

## Run

```sh
./build/apps/querylume_cli/querylume run \
  --data examples/stocks.json \
  --pipeline examples/top_stocks.pipeline.json
```

```json
[
  { "price": 506.2, "symbol": "MSFT" },
  { "price": 211.5, "symbol": "AAPL" }
]
```

### EXPLAIN

```sh
./build/apps/querylume_cli/querylume explain \
  --data examples/stocks.json \
  --pipeline examples/top_stocks.pipeline.json
```

Prints the parsed pipeline, the logical plan, the optimized logical plan,
the optimizer rewrite trace, and the physical plan shape, as JSON. Does
**not** execute the physical plan (no per-stage statistics are included).

```json
{
  "parsedPipeline": [ ... ],
  "logicalPlan": { "node": "Limit", "count": 10, "child": { "node": "Sort", ... } },
  "optimizedLogicalPlan": { "node": "TopK", "field": "price", "direction": -1, "k": 10, "child": { ... } },
  "rewrites": [ { "rule": "SortLimitToTopK", "applied": true } ],
  "physicalPlan": { "stage": "TopKStage", "child": { "stage": "ProjectStage", ... } }
}
```

### EXPLAIN ANALYZE

```sh
./build/apps/querylume_cli/querylume explain-analyze \
  --data examples/stocks.json \
  --pipeline examples/top_stocks.pipeline.json \
  --with-result
```

Same shape as EXPLAIN, plus per-stage runtime statistics embedded in
`physicalPlan`, a `globalStats` object (`documentsExamined`,
`documentsReturned`, `totalExecutionTimeMicros`,
`optimizerRulesApplied`), and (with `--with-result`) the query result rows
under `result`.

## Architecture overview

- **Parser** (`querylume/parser`): validates JSON *shape*, produces
  `UnresolvedFieldExpression`s. Does not touch the schema.
- **Binder** (`querylume/binder`): resolves fields/operators/directions
  against the schema, threading it through the pipeline in stage order.
  Produces the logical plan.
- **Logical plan** (`querylume/logical`): `LogicalScan`/`Filter`/
  `Project`/`Sort`/`Limit`/`TopK`, pure data, no execution.
- **Optimizer** (`querylume/optimizer`): one rule, `SortLimitToTopKRule`,
  run by a small `Optimizer` driver that records a rewrite trace.
- **Physical planner** (`querylume/physical/physical_planner.h`): explicitly
  consumes the optimized logical plan and lowers it 1:1 into an independently
  owned `PlanStage` tree.
- **Execution** (`querylume/physical`): Volcano-style pull iterators
  (`open`/`getNext`/`close`), each owning its child via `unique_ptr`;
  an RAII guard guarantees closure on success and exceptions.
- **Explain** (`querylume/explain`): JSON serialization for every stage of
  the above, plus the `run`/`explain`/`explain-analyze` orchestration used
  by the CLI.

Full detail in `docs/architecture.md`.

## Complexity

| Stage | Time | Additional space |
|---|---|---|
| `CollectionScanStage` | O(n) | O(1) |
| `FilterStage` | O(n) | O(1) |
| `ProjectStage` | O(n) | O(1) |
| `LimitStage` | O(k) | O(1) |
| `SortStage` | O(n log n) | O(n) |
| `TopKStage` | O(n log k) | O(k) |

`n` = input row count reaching the stage, `k` = limit/top-k count.

## Current limitations

- Single sort field only (no multi-key sort).
- `$match` supports only implicit AND of simple field/literal comparisons
  (no `$or`, `$not`, nested predicates, or field-to-field comparison).
- No static type checking of literal/field compatibility at bind time
  (the data model has no per-column static type); incompatible comparisons
  are caught at row-evaluation time instead. See `docs/decisions.md`.
- `peak_memory_bytes` is an approximate, deterministic content-based
  estimate, not actual allocator bytes.
- Stage execution time is inclusive of descendants; there is no per-stage
  "self time" without manually subtracting a child's reported time.
- Single-threaded, in-memory, single-process only.

## Future roadmap (explicitly not in this MVP)

Cost-based optimization, filter pushdown, projection pruning,
aggregation/`$group`, joins, indexes, persistence, and any of the other
items listed in "Explicit non-goals" in the project spec would be
candidates for a Phase 2, each requiring its own design discussion rather
than being bolted onto this MVP's boundary.

## Testing

```sh
ctest --test-dir build
```

Covers: data model (schema inference, null-filling, nested-value
rejection, mixed numeric types), expression evaluation (all comparison
operators, null/incompatible-type semantics, conjunction), parser/binder
(valid pipelines and every documented error category), each physical stage
in isolation (including empty input, `k == 0`, `k > n`, deterministic
ties), randomized Sort+Limit-vs-TopK equivalence with nulls and mixed
numerics, CLI exit/stdout/stderr behavior, the optimizer rule
(adjacent/non-adjacent/missing-half cases, and a
rewrite that occurs below the plan root), an optimized-vs-unoptimized
integration test asserting identical output, and a golden-file test
pinning EXPLAIN's JSON shape.

## Benchmark

```sh
cmake --build build --target sort_vs_topk_benchmark
./build/benchmarks/sort_vs_topk_benchmark
```

Compares `SortStage` + `LimitStage` against `TopKStage` across
`n in {1000, 10000, 100000}` and `k in {10, 100}`. See the final report in
this repository's implementation notes for observed numbers on the
machine this was built on -- do not treat those numbers as portable
guarantees; re-run locally for your own hardware.
