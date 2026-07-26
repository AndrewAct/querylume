#include "querylume/optimizer/sort_limit_to_topk_rule.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "querylume/binder/binder.h"
#include "querylume/logical/logical_filter.h"
#include "querylume/logical/logical_limit.h"
#include "querylume/logical/logical_project.h"
#include "querylume/logical/logical_sort.h"
#include "querylume/logical/logical_topk.h"
#include "querylume/optimizer/optimization_context.h"
#include "querylume/parser/pipeline_parser.h"

namespace querylume {
namespace {

std::shared_ptr<Table> makeTable() {
    auto table = std::make_shared<Table>();
    table->schema = Schema({"symbol", "price", "sector"});
    table->rows.push_back(Row{0, {std::string("AAPL"), 211.5, std::string("technology")}});
    return table;
}

std::unique_ptr<LogicalPlanNode> bind(const std::string& pipeline_text) {
    auto json = nlohmann::json::parse(pipeline_text);
    ParsedPipeline parsed = parsePipeline(json);
    return bindPipeline(makeTable(), parsed);
}

TEST(SortLimitToTopKRuleTest, AdjacentSortLimitRewritesToTopK) {
    auto root = bind(R"([{"$sort": {"price": -1}}, {"$limit": 10}])");
    ASSERT_EQ(root->kind(), LogicalNodeKind::kLimit);

    SortLimitToTopKRule rule;
    OptimizationContext context;
    const bool applied = rule.apply(root, context);

    EXPECT_TRUE(applied);
    ASSERT_EQ(context.trace.size(), 1u);
    EXPECT_EQ(context.trace[0].rule, "SortLimitToTopK");
    EXPECT_TRUE(context.trace[0].applied);

    ASSERT_EQ(root->kind(), LogicalNodeKind::kTopK);
    const auto& topk = static_cast<const LogicalTopK&>(*root);
    EXPECT_EQ(topk.fieldName(), "price");
    EXPECT_EQ(topk.direction(), SortDirection::kDescending);
    EXPECT_EQ(topk.k(), 10u);
}

TEST(SortLimitToTopKRuleTest, SortWithoutLimitDoesNotRewrite) {
    auto root = bind(R"([{"$sort": {"price": -1}}])");
    ASSERT_EQ(root->kind(), LogicalNodeKind::kSort);

    SortLimitToTopKRule rule;
    OptimizationContext context;
    const bool applied = rule.apply(root, context);

    EXPECT_FALSE(applied);
    EXPECT_FALSE(context.trace[0].applied);
    EXPECT_EQ(root->kind(), LogicalNodeKind::kSort);
}

TEST(SortLimitToTopKRuleTest, LimitWithoutSortDoesNotRewrite) {
    auto root = bind(R"([{"$limit": 10}])");
    ASSERT_EQ(root->kind(), LogicalNodeKind::kLimit);

    SortLimitToTopKRule rule;
    OptimizationContext context;
    const bool applied = rule.apply(root, context);

    EXPECT_FALSE(applied);
    EXPECT_EQ(root->kind(), LogicalNodeKind::kLimit);
}

TEST(SortLimitToTopKRuleTest, NonAdjacentSortAndLimitDoNotRewrite) {
    // $sort, $project, $limit -> Limit's direct child is Project, not Sort.
    auto root = bind(R"([
        {"$sort": {"price": -1}},
        {"$project": ["symbol", "price"]},
        {"$limit": 10}
    ])");
    ASSERT_EQ(root->kind(), LogicalNodeKind::kLimit);

    SortLimitToTopKRule rule;
    OptimizationContext context;
    const bool applied = rule.apply(root, context);

    EXPECT_FALSE(applied);
    ASSERT_EQ(root->kind(), LogicalNodeKind::kLimit);
    const auto& limit_node = static_cast<const LogicalLimit&>(*root);
    EXPECT_EQ(limit_node.child().kind(), LogicalNodeKind::kProject);
}

TEST(SortLimitToTopKRuleTest, RewriteDeepInChainNotJustAtRoot) {
    // $sort, $limit, $project -> root is Project; Limit-over-Sort is one
    // level down and must still be found and rewritten.
    auto root = bind(R"([
        {"$sort": {"price": -1}},
        {"$limit": 10},
        {"$project": ["symbol", "price"]}
    ])");
    ASSERT_EQ(root->kind(), LogicalNodeKind::kProject);

    SortLimitToTopKRule rule;
    OptimizationContext context;
    const bool applied = rule.apply(root, context);

    EXPECT_TRUE(applied);
    const auto& project_node = static_cast<const LogicalProject&>(*root);
    EXPECT_EQ(project_node.child().kind(), LogicalNodeKind::kTopK);
}

}  // namespace
}  // namespace querylume
