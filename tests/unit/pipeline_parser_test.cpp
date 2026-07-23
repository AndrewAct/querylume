#include "querylume/parser/pipeline_parser.h"

#include <gtest/gtest.h>

#include <limits>
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
    try {
        static_cast<void>(parsePipeline(json));
        FAIL() << "expected QueryLumeError";
    } catch (const QueryLumeError& error) {
        EXPECT_EQ(error.code(), ErrorCode::kUnknownStage);
        EXPECT_NE(std::string(error.what()).find("$group"), std::string::npos);
    }
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

TEST(PipelineParserTest, RejectsComparisonIntegerOutsideInt64Range) {
    nlohmann::json pipeline = nlohmann::json::array();
    pipeline.push_back({{"$match", {{"value", {{"$eq", std::numeric_limits<std::uint64_t>::max()}}}}}});
    try {
        static_cast<void>(parsePipeline(pipeline));
        FAIL() << "expected QueryLumeError";
    } catch (const QueryLumeError& error) {
        EXPECT_EQ(error.code(), ErrorCode::kUnsupportedValueType);
        EXPECT_NE(std::string(error.what()).find("outside the int64 range"), std::string::npos);
    }
}

TEST(PipelineParserTest, RejectsLimitOutsideInt64Range) {
    nlohmann::json pipeline =
        nlohmann::json::array({{{"$limit", std::numeric_limits<std::uint64_t>::max()}}});
    try {
        static_cast<void>(parsePipeline(pipeline));
        FAIL() << "expected QueryLumeError";
    } catch (const QueryLumeError& error) {
        EXPECT_EQ(error.code(), ErrorCode::kPipelineSyntaxError);
        EXPECT_NE(std::string(error.what()).find("signed 64-bit"), std::string::npos);
    }
}

}  // namespace
}  // namespace querylume
