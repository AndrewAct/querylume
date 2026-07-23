#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "querylume/binder/binder.h"
#include "querylume/data/json_loader.h"
#include "querylume/optimizer/optimizer.h"
#include "querylume/parser/pipeline_parser.h"
#include "querylume/physical/physical_planner.h"

namespace querylume {
namespace {

const char* kData = R"([
    {"symbol": "AAPL", "price": 211.5, "volume": 1000, "sector": "technology"},
    {"symbol": "MSFT", "price": 506.2, "volume": 800, "sector": "technology"},
    {"symbol": "JPM", "price": 289.4, "volume": 1200, "sector": "finance"},
    {"symbol": "GOOG", "price": 178.3, "volume": 1500, "sector": "technology"},
    {"symbol": "XOM", "price": 118.9, "volume": 2000, "sector": "energy"}
])";

const char* kPipeline = R"([
    {"$match": {"sector": {"$eq": "technology"}, "price": {"$gte": 100}}},
    {"$project": ["symbol", "price"]},
    {"$sort": {"price": -1}},
    {"$limit": 2}
])";

std::vector<Row> execute(std::unique_ptr<PlanStage> stage) {
    std::vector<Row> rows;
    stage->open();
    Row row;
    while (stage->getNext(row) == StageState::kAdvanced) {
        rows.push_back(row);
    }
    stage->close();
    return rows;
}

TEST(OptimizedVsUnoptimizedTest, ProduceIdenticalRowsAndOrder) {
    auto table = std::make_shared<Table>(loadTableFromJsonText(kData));
    auto pipeline_json = nlohmann::json::parse(kPipeline);
    ParsedPipeline parsed = parsePipeline(pipeline_json);

    auto unoptimized_root = bindPipeline(table, parsed);
    ASSERT_EQ(unoptimized_root->kind(), LogicalNodeKind::kLimit);
    auto unoptimized_rows = execute(buildPhysicalPlan(std::move(unoptimized_root)));

    auto optimized_root = bindPipeline(table, parsed);
    Optimizer optimizer;
    OptimizationContext context = optimizer.optimize(optimized_root);
    ASSERT_EQ(optimized_root->kind(), LogicalNodeKind::kTopK);
    ASSERT_EQ(context.trace.size(), 1u);
    EXPECT_TRUE(context.trace[0].applied);
    auto optimized_rows = execute(buildPhysicalPlan(std::move(optimized_root)));

    ASSERT_EQ(optimized_rows.size(), unoptimized_rows.size());
    for (std::size_t i = 0; i < optimized_rows.size(); ++i) {
        EXPECT_EQ(optimized_rows[i].ordinal, unoptimized_rows[i].ordinal);
        EXPECT_EQ(std::get<std::string>(optimized_rows[i].values[0]),
                  std::get<std::string>(unoptimized_rows[i].values[0]));
        EXPECT_EQ(std::get<double>(optimized_rows[i].values[1]),
                  std::get<double>(unoptimized_rows[i].values[1]));
    }

    ASSERT_EQ(unoptimized_rows.size(), 2u);
    EXPECT_EQ(std::get<std::string>(unoptimized_rows[0].values[0]), "MSFT");
    EXPECT_EQ(std::get<std::string>(unoptimized_rows[1].values[0]), "AAPL");
}

}  // namespace
}  // namespace querylume
