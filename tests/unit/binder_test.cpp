#include "querylume/binder/binder.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "querylume/common/error.h"
#include "querylume/logical/logical_filter.h"
#include "querylume/logical/logical_limit.h"
#include "querylume/logical/logical_project.h"
#include "querylume/logical/logical_scan.h"
#include "querylume/logical/logical_sort.h"
#include "querylume/parser/pipeline_parser.h"

namespace querylume {
namespace {

std::shared_ptr<Table> makeTable() {
    auto table = std::make_shared<Table>();
    table->schema = Schema({"symbol", "price", "sector"});
    table->rows.push_back(Row{0, {std::string("AAPL"), 211.5, std::string("technology")}});
    return table;
}

std::unique_ptr<LogicalPlanNode> bind(const std::string& pipeline_text,
                                      std::shared_ptr<Table> table = nullptr) {
    if (!table) table = makeTable();
    auto json = nlohmann::json::parse(pipeline_text);
    ParsedPipeline parsed = parsePipeline(json);
    return bindPipeline(table, parsed);
}

TEST(BinderTest, ValidPipelineProducesExpectedTreeShape) {
    auto root = bind(R"([
        {"$match": {"sector": {"$eq": "technology"}, "price": {"$gte": 200}}},
        {"$project": ["symbol", "price"]},
        {"$sort": {"price": -1}},
        {"$limit": 10}
    ])");

    ASSERT_EQ(root->kind(), LogicalNodeKind::kLimit);
    auto& limit_node = static_cast<LogicalLimit&>(*root);
    EXPECT_EQ(limit_node.limit(), 10u);

    ASSERT_EQ(limit_node.child().kind(), LogicalNodeKind::kSort);
    const auto& sort_node = static_cast<const LogicalSort&>(limit_node.child());
    EXPECT_EQ(sort_node.fieldName(), "price");
    EXPECT_EQ(sort_node.direction(), SortDirection::kDescending);

    ASSERT_EQ(sort_node.child().kind(), LogicalNodeKind::kProject);
    const auto& project_node = static_cast<const LogicalProject&>(sort_node.child());
    EXPECT_EQ(project_node.fieldNames(), (std::vector<std::string>{"symbol", "price"}));

    ASSERT_EQ(project_node.child().kind(), LogicalNodeKind::kFilter);
    const auto& filter_node = static_cast<const LogicalFilter&>(project_node.child());
    EXPECT_EQ(filter_node.child().kind(), LogicalNodeKind::kScan);
}

TEST(BinderTest, UnknownOperatorThrows) {
    EXPECT_THROW(bind(R"([{"$match": {"price": {"$between": 200}}}])"), QueryLumeError);
}

TEST(BinderTest, UnknownFieldInMatchThrows) {
    EXPECT_THROW(bind(R"([{"$match": {"nope": {"$eq": 1}}}])"), QueryLumeError);
}

TEST(BinderTest, InvalidProjectionFieldThrows) {
    EXPECT_THROW(bind(R"([{"$project": ["nope"]}])"), QueryLumeError);
}

TEST(BinderTest, SortFieldMustExist) { EXPECT_THROW(bind(R"([{"$sort": {"nope": 1}}])"), QueryLumeError); }

TEST(BinderTest, InvalidSortDirectionThrows) {
    EXPECT_THROW(bind(R"([{"$sort": {"price": 2}}])"), QueryLumeError);
}

TEST(BinderTest, NegativeLimitThrows) { EXPECT_THROW(bind(R"([{"$limit": -1}])"), QueryLumeError); }

TEST(BinderTest, SortAfterProjectResolvesAgainstProjectedSchema) {
    auto root = bind(R"([
        {"$project": ["symbol", "price"]},
        {"$sort": {"price": 1}}
    ])");
    ASSERT_EQ(root->kind(), LogicalNodeKind::kSort);
    EXPECT_NO_THROW(bind(R"([{"$project": ["symbol", "price"]}, {"$sort": {"price": 1}}])"));
    EXPECT_THROW(bind(R"([{"$project": ["symbol", "price"]}, {"$sort": {"sector": 1}}])"), QueryLumeError);
}

}  // namespace
}  // namespace querylume
