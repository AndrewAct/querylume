#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "querylume/data/json_loader.h"
#include "querylume/explain/query_facade.h"

namespace querylume {
namespace {

const char* kData = R"([
    {"symbol": "AAPL", "price": 211.5, "volume": 1000, "sector": "technology"},
    {"symbol": "MSFT", "price": 506.2, "volume": 800, "sector": "technology"},
    {"symbol": "JPM", "price": 289.4, "volume": 1200, "sector": "finance"}
])";

const char* kPipeline = R"([
    {"$match": {"sector": {"$eq": "technology"}, "price": {"$gte": 200}}},
    {"$project": ["symbol", "price"]},
    {"$sort": {"price": -1}},
    {"$limit": 10}
])";

// This is the exact expected shape for EXPLAIN's stable output. If this
// test fails after an intentional serialization change, update the golden
// value below in the same commit as the change.
const char* kExpectedExplain = R"({
  "logicalPlan": {
    "child": {
      "child": {
        "child": {
          "child": {
            "fields": [
              "price",
              "sector",
              "symbol",
              "volume"
            ],
            "node": "Scan",
            "rowCount": 3
          },
          "node": "Filter",
          "predicate": "price $gte 200 AND sector $eq \"technology\""
        },
        "fields": [
          "symbol",
          "price"
        ],
        "node": "Project"
      },
      "direction": -1,
      "field": "price",
      "node": "Sort"
    },
    "count": 10,
    "node": "Limit"
  },
  "optimizedLogicalPlan": {
    "child": {
      "child": {
        "child": {
          "fields": [
            "price",
            "sector",
            "symbol",
            "volume"
          ],
          "node": "Scan",
          "rowCount": 3
        },
        "node": "Filter",
        "predicate": "price $gte 200 AND sector $eq \"technology\""
      },
      "fields": [
        "symbol",
        "price"
      ],
      "node": "Project"
    },
    "direction": -1,
    "field": "price",
    "k": 10,
    "node": "TopK"
  },
  "parsedPipeline": [
    {
      "predicates": [
        {
          "field": "price",
          "literal": 200,
          "operator": "$gte"
        },
        {
          "field": "sector",
          "literal": "technology",
          "operator": "$eq"
        }
      ],
      "stage": "$match"
    },
    {
      "fields": [
        "symbol",
        "price"
      ],
      "stage": "$project"
    },
    {
      "direction": -1,
      "field": "price",
      "stage": "$sort"
    },
    {
      "count": 10,
      "stage": "$limit"
    }
  ],
  "physicalPlan": {
    "child": {
      "child": {
        "child": {
          "stage": "CollectionScanStage"
        },
        "stage": "FilterStage"
      },
      "stage": "ProjectStage"
    },
    "stage": "TopKStage"
  },
  "rewrites": [
    {
      "applied": true,
      "rule": "SortLimitToTopK"
    }
  ]
})";

TEST(ExplainGoldenTest, MatchesStableSnapshot) {
    auto table = std::make_shared<Table>(loadTableFromJsonText(kData));
    auto pipeline_json = nlohmann::json::parse(kPipeline);

    nlohmann::json actual = explainPipeline(table, pipeline_json);
    nlohmann::json expected = nlohmann::json::parse(kExpectedExplain);

    EXPECT_EQ(actual, expected) << "actual:\n" << actual.dump(2);
}

TEST(ExplainGoldenTest, ExplainDoesNotExecutePhysicalPlan) {
    auto table = std::make_shared<Table>(loadTableFromJsonText(kData));
    auto pipeline_json = nlohmann::json::parse(kPipeline);

    nlohmann::json actual = explainPipeline(table, pipeline_json);
    // EXPLAIN must not execute: no stats key anywhere in physicalPlan.
    EXPECT_FALSE(actual["physicalPlan"].contains("stats"));
    EXPECT_FALSE(actual["physicalPlan"]["child"].contains("stats"));
}

TEST(ExplainAnalyzeGoldenTest, ContainsNonZeroStageCountersAndMatchesResultSize) {
    auto table = std::make_shared<Table>(loadTableFromJsonText(kData));
    auto pipeline_json = nlohmann::json::parse(kPipeline);

    nlohmann::json actual = explainAnalyzePipeline(table, pipeline_json, /*include_results=*/true);

    ASSERT_TRUE(actual["physicalPlan"].contains("stats"));
    EXPECT_GT(actual["physicalPlan"]["stats"]["rowsOut"].get<std::uint64_t>(), 0u);
    EXPECT_GT(actual["physicalPlan"]["stats"]["getNextCalls"].get<std::uint64_t>(), 0u);

    EXPECT_EQ(actual["globalStats"]["documentsExamined"].get<std::uint64_t>(), 3u);
    EXPECT_EQ(actual["globalStats"]["documentsReturned"].get<std::uint64_t>(), actual["result"].size());
    EXPECT_EQ(actual["globalStats"]["optimizerRulesApplied"].get<std::uint64_t>(), 1u);
}

}  // namespace
}  // namespace querylume
