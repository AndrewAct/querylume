#include <benchmark/benchmark.h>

#include <memory>
#include <random>

#include "querylume/data/table.h"
#include "querylume/physical/collection_scan_stage.h"
#include "querylume/physical/limit_stage.h"
#include "querylume/physical/plan_stage.h"
#include "querylume/physical/sort_stage.h"
#include "querylume/physical/topk_stage.h"

namespace {

std::shared_ptr<querylume::Table> makeTable(std::size_t n) {
    auto table = std::make_shared<querylume::Table>();
    table->schema = querylume::Schema({"value"});
    table->rows.reserve(n);

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int64_t> dist(0, static_cast<int64_t>(n) * 10);
    for (std::size_t i = 0; i < n; ++i) {
        querylume::Row row;
        row.ordinal = i;
        row.values.push_back(dist(rng));
        table->rows.push_back(std::move(row));
    }
    return table;
}

void drain(querylume::PlanStage& stage) {
    querylume::Row row;
    while (stage.getNext(row) == querylume::StageState::kAdvanced) {
        benchmark::DoNotOptimize(row);
    }
}

void BM_SortPlusLimit(benchmark::State& state) {
    const auto n = static_cast<std::size_t>(state.range(0));
    const auto k = static_cast<std::uint64_t>(state.range(1));
    auto table = makeTable(n);

    for (auto _ : state) {
        auto scan = std::make_unique<querylume::CollectionScanStage>(table);
        auto sort_stage =
            std::make_unique<querylume::SortStage>(std::move(scan), 0, querylume::SortDirection::kDescending);
        querylume::LimitStage limit_stage(std::move(sort_stage), k);
        limit_stage.open();
        drain(limit_stage);
        limit_stage.close();
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

void BM_TopK(benchmark::State& state) {
    const auto n = static_cast<std::size_t>(state.range(0));
    const auto k = static_cast<std::uint64_t>(state.range(1));
    auto table = makeTable(n);

    for (auto _ : state) {
        auto scan = std::make_unique<querylume::CollectionScanStage>(table);
        querylume::TopKStage topk_stage(std::move(scan), 0, querylume::SortDirection::kDescending, k);
        topk_stage.open();
        drain(topk_stage);
        topk_stage.close();
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

void registerArgs(benchmark::internal::Benchmark* b) {
    for (int64_t n : {1000, 10000, 100000}) {
        for (int64_t k : {10, 100}) {
            b->Args({n, k});
        }
    }
}

}  // namespace

BENCHMARK(BM_SortPlusLimit)->Apply(registerArgs);
BENCHMARK(BM_TopK)->Apply(registerArgs);
