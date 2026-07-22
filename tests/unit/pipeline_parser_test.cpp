#include "querylume/parser/pipeline_parser.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "querylume/common/error.h"

namespace querylume {
namespace {

TEST(PipelineParserTest, ParsesValidPipeline) {
    auto json = nlohmann::json::parse(R"([
        {"$match": {"sector": {"$eq": "technology"}, "price": {"$gte": 200}}},
        {"$project": ["symbol", "price"]},
        {"$sort": {"price": -1}},
        {"$limit": 10}
    ])");
    ParsedPipeline pipeline = parsePipeline(json);
    ASSERT_EQ(pipeline.stages.size(), 4u);
    EXPECT_EQ(pipeline.stages[0].kind, ParsedStageKind::kMatch);
    EXPECT_EQ(pipeline.stages[0].match.predicates.size(), 2u);
    EXPECT_EQ(pipeline.stages[1].kind, ParsedStageKind::kProject);
    EXPECT_EQ(pipeline.stages[1].project.fields.size(), 2u);
    EXPECT_EQ(pipeline.stages[2].kind, ParsedStageKind::kSort);
    EXPECT_EQ(pipeline.stages[2].sort.direction_raw, -1);
    EXPECT_EQ(pipeline.stages[3].kind, ParsedStageKind::kLimit);
    EXPECT_EQ(pipeline.stages[3].limit.count_raw, 10);
}

TEST(PipelineParserTest, UnknownStageThrows) {
    auto json = nlohmann::json::parse(R"([{"$group": {}}])");
    EXPECT_THROW(parsePipeline(json), QueryLumeError);
}

TEST(PipelineParserTest, MatchPredicateMustBeSingleKeyObject) {
    auto json = nlohmann::json::parse(R"([{"$match": {"price": 200}}])");
    EXPECT_THROW(parsePipeline(json), QueryLumeError);
}

TEST(PipelineParserTest, ProjectMustBeArrayOfStrings) {
    auto json = nlohmann::json::parse(R"([{"$project": [1, 2]}])");
    EXPECT_THROW(parsePipeline(json), QueryLumeError);
}

TEST(PipelineParserTest, SortMustBeSingleKeyObject) {
    auto json = nlohmann::json::parse(R"([{"$sort": {"a": 1, "b": -1}}])");
    EXPECT_THROW(parsePipeline(json), QueryLumeError);
}

TEST(PipelineParserTest, LimitMustBeInteger) {
    auto json = nlohmann::json::parse(R"([{"$limit": "ten"}])");
    EXPECT_THROW(parsePipeline(json), QueryLumeError);
}

TEST(PipelineParserTest, PipelineMustBeArray) {
    auto json = nlohmann::json::parse(R"({"$limit": 10})");
    EXPECT_THROW(parsePipeline(json), QueryLumeError);
}

}  // namespace
}  // namespace querylume
