#include <gtest/gtest.h>

#include <memory>
#include <random>

#include "querylume/common/error.h"
#include "querylume/expression/bound_field_expression.h"
#include "querylume/expression/comparison_expression.h"
#include "querylume/expression/literal_expression.h"
#include "querylume/physical/collection_scan_stage.h"
#include "querylume/physical/filter_stage.h"
#include "querylume/physical/limit_stage.h"
#include "querylume/physical/plan_stage_execution_guard.h"
#include "querylume/physical/project_stage.h"
#include "querylume/physical/sort_stage.h"
#include "querylume/physical/topk_stage.h"

namespace querylume {
namespace {

std::shared_ptr<Table> makeTable() {
    auto table = std::make_shared<Table>();
    table->schema = Schema({"symbol", "price", "volume"});
    table->rows.push_back(Row{0, {std::string("AAPL"), 211.5, int64_t{1000}}});
    table->rows.push_back(Row{1, {std::string("MSFT"), 506.2, int64_t{800}}});
    table->rows.push_back(Row{2, {std::string("JPM"), 289.4, int64_t{1200}}});
    return table;
}

std::vector<Row> drain(PlanStage& stage) {
    std::vector<Row> rows;
    Row row;
    while (stage.getNext(row) == StageState::kAdvanced) {
        rows.push_back(row);
    }
    return rows;
}

TEST(CollectionScanStageTest, EmitsRowsInOriginalOrder) {
    CollectionScanStage stage(makeTable());
    stage.open();
    auto rows = drain(stage);
    stage.close();

    ASSERT_EQ(rows.size(), 3u);
    EXPECT_EQ(std::get<std::string>(rows[0].values[0]), "AAPL");
    EXPECT_EQ(std::get<std::string>(rows[1].values[0]), "MSFT");
    EXPECT_EQ(std::get<std::string>(rows[2].values[0]), "JPM");
    EXPECT_EQ(stage.stats().rows_out, 3u);
}

TEST(CollectionScanStageTest, EmptyInput) {
    auto table = std::make_shared<Table>();
    table->schema = Schema({"a"});
    CollectionScanStage stage(table);
    stage.open();
    Row row;
    EXPECT_EQ(stage.getNext(row), StageState::kEof);
    EXPECT_EQ(stage.getNext(row), StageState::kEof);  // idempotent EOF
    stage.close();
}

TEST(CollectionScanStageTest, LifecycleViolations) {
    CollectionScanStage stage(makeTable());
    Row row;
    EXPECT_THROW(stage.getNext(row), QueryLumeError);  // before open()
    stage.open();
    EXPECT_THROW(stage.open(), QueryLumeError);  // double open()
    stage.close();
    EXPECT_THROW(stage.getNext(row), QueryLumeError);  // after close()
    EXPECT_THROW(stage.open(), QueryLumeError);        // reopen unsupported
    stage.close();                                     // idempotent close()
}

TEST(FilterStageTest, EmitsOnlyMatchingRows) {
    auto predicate = std::make_unique<ComparisonExpression>(
        std::make_unique<BoundFieldExpression>(1, "price"), ComparisonOperator::kGte,
        std::make_unique<LiteralExpression>(Value(300.0)));
    auto scan = std::make_unique<CollectionScanStage>(makeTable());
    FilterStage stage(std::move(scan), std::move(predicate));
    stage.open();
    auto rows = drain(stage);
    stage.close();

    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<std::string>(rows[0].values[0]), "MSFT");
    EXPECT_EQ(stage.stats().rows_in, 3u);
    EXPECT_EQ(stage.stats().rows_out, 1u);
}

TEST(ProjectStageTest, EmitsSelectedColumnsAndPreservesOrdinal) {
    auto scan = std::make_unique<CollectionScanStage>(makeTable());
    ProjectStage stage(std::move(scan), std::vector<std::size_t>{0, 1});
    stage.open();
    auto rows = drain(stage);
    stage.close();

    ASSERT_EQ(rows.size(), 3u);
    EXPECT_EQ(rows[0].values.size(), 2u);
    EXPECT_EQ(rows[0].ordinal, 0u);
    EXPECT_EQ(std::get<std::string>(rows[0].values[0]), "AAPL");
}

TEST(LimitStageTest, StopsAfterKRows) {
    auto scan = std::make_unique<CollectionScanStage>(makeTable());
    CollectionScanStage* scan_observer = scan.get();
    LimitStage stage(std::move(scan), 2);
    stage.open();
    auto rows = drain(stage);
    stage.close();
    EXPECT_EQ(rows.size(), 2u);
    EXPECT_EQ(scan_observer->stats().rows_out, 2u);
}

TEST(LimitStageTest, ZeroLimitReturnsEofImmediately) {
    auto scan = std::make_unique<CollectionScanStage>(makeTable());
    CollectionScanStage* scan_observer = scan.get();
    LimitStage stage(std::move(scan), 0);
    stage.open();
    Row row;
    EXPECT_EQ(stage.getNext(row), StageState::kEof);
    stage.close();
    EXPECT_EQ(scan_observer->stats().rows_out, 0u);
}

TEST(SortStageTest, SortsAscending) {
    auto scan = std::make_unique<CollectionScanStage>(makeTable());
    SortStage stage(std::move(scan), 1, SortDirection::kAscending);
    stage.open();
    auto rows = drain(stage);
    stage.close();

    ASSERT_EQ(rows.size(), 3u);
    EXPECT_EQ(std::get<double>(rows[0].values[1]), 211.5);
    EXPECT_EQ(std::get<double>(rows[1].values[1]), 289.4);
    EXPECT_EQ(std::get<double>(rows[2].values[1]), 506.2);
}

TEST(SortStageTest, SortsDescending) {
    auto scan = std::make_unique<CollectionScanStage>(makeTable());
    SortStage stage(std::move(scan), 1, SortDirection::kDescending);
    stage.open();
    auto rows = drain(stage);
    stage.close();

    ASSERT_EQ(rows.size(), 3u);
    EXPECT_EQ(std::get<double>(rows[0].values[1]), 506.2);
    EXPECT_EQ(std::get<double>(rows[2].values[1]), 211.5);
}

TEST(SortStageTest, DeterministicTiesUseOrdinal) {
    auto table = std::make_shared<Table>();
    table->schema = Schema({"v"});
    table->rows.push_back(Row{0, {int64_t{5}}});
    table->rows.push_back(Row{1, {int64_t{5}}});
    table->rows.push_back(Row{2, {int64_t{5}}});

    auto scan = std::make_unique<CollectionScanStage>(table);
    SortStage stage(std::move(scan), 0, SortDirection::kAscending);
    stage.open();
    auto rows = drain(stage);
    stage.close();

    ASSERT_EQ(rows.size(), 3u);
    EXPECT_EQ(rows[0].ordinal, 0u);
    EXPECT_EQ(rows[1].ordinal, 1u);
    EXPECT_EQ(rows[2].ordinal, 2u);
}

TEST(SortStageTest, NullsSortAfterInAscendingAndBeforeInDescending) {
    auto table = std::make_shared<Table>();
    table->schema = Schema({"v"});
    table->rows.push_back(Row{0, {int64_t{5}}});
    table->rows.push_back(Row{1, {nullptr}});
    table->rows.push_back(Row{2, {int64_t{1}}});

    {
        auto scan = std::make_unique<CollectionScanStage>(table);
        SortStage stage(std::move(scan), 0, SortDirection::kAscending);
        stage.open();
        auto rows = drain(stage);
        stage.close();
        ASSERT_EQ(rows.size(), 3u);
        EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(rows.back().values[0]));
    }
    {
        auto scan = std::make_unique<CollectionScanStage>(table);
        SortStage stage(std::move(scan), 0, SortDirection::kDescending);
        stage.open();
        auto rows = drain(stage);
        stage.close();
        ASSERT_EQ(rows.size(), 3u);
        EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(rows.front().values[0]));
    }
}

std::shared_ptr<Table> makeLargerTable() {
    auto table = std::make_shared<Table>();
    table->schema = Schema({"v"});
    const std::vector<int64_t> values = {5, 1, 9, 3, 7, 2, 8, 4, 6, 0};
    for (std::size_t i = 0; i < values.size(); ++i) {
        table->rows.push_back(Row{static_cast<std::uint64_t>(i), {values[i]}});
    }
    return table;
}

TEST(TopKStageTest, MatchesSortPlusLimitAscending) {
    auto table = makeLargerTable();

    auto scan1 = std::make_unique<CollectionScanStage>(table);
    SortStage sort_stage(std::move(scan1), 0, SortDirection::kAscending);
    sort_stage.open();
    std::vector<Row> expected;
    Row row;
    int count = 0;
    while (count < 3 && sort_stage.getNext(row) == StageState::kAdvanced) {
        expected.push_back(row);
        ++count;
    }
    sort_stage.close();

    auto scan2 = std::make_unique<CollectionScanStage>(table);
    TopKStage topk_stage(std::move(scan2), 0, SortDirection::kAscending, 3);
    topk_stage.open();
    auto actual = drain(topk_stage);
    topk_stage.close();

    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t i = 0; i < actual.size(); ++i) {
        EXPECT_EQ(std::get<int64_t>(actual[i].values[0]), std::get<int64_t>(expected[i].values[0]));
        EXPECT_EQ(actual[i].ordinal, expected[i].ordinal);
    }
}

TEST(TopKStageTest, MatchesSortPlusLimitDescending) {
    auto table = makeLargerTable();

    auto scan2 = std::make_unique<CollectionScanStage>(table);
    TopKStage topk_stage(std::move(scan2), 0, SortDirection::kDescending, 4);
    topk_stage.open();
    auto actual = drain(topk_stage);
    topk_stage.close();

    ASSERT_EQ(actual.size(), 4u);
    EXPECT_EQ(std::get<int64_t>(actual[0].values[0]), 9);
    EXPECT_EQ(std::get<int64_t>(actual[1].values[0]), 8);
    EXPECT_EQ(std::get<int64_t>(actual[2].values[0]), 7);
    EXPECT_EQ(std::get<int64_t>(actual[3].values[0]), 6);
}

TEST(TopKStageTest, KEqualsZero) {
    auto table = makeLargerTable();
    auto scan = std::make_unique<CollectionScanStage>(table);
    TopKStage stage(std::move(scan), 0, SortDirection::kAscending, 0);
    stage.open();
    Row row;
    EXPECT_EQ(stage.getNext(row), StageState::kEof);
    stage.close();
}

TEST(TopKStageTest, KGreaterThanN) {
    auto table = makeLargerTable();
    auto scan = std::make_unique<CollectionScanStage>(table);
    TopKStage stage(std::move(scan), 0, SortDirection::kAscending, 1000);
    stage.open();
    auto rows = drain(stage);
    stage.close();
    EXPECT_EQ(rows.size(), table->rows.size());
}

TEST(TopKStageTest, EmptyInput) {
    auto table = std::make_shared<Table>();
    table->schema = Schema({"v"});
    auto scan = std::make_unique<CollectionScanStage>(table);
    TopKStage stage(std::move(scan), 0, SortDirection::kAscending, 5);
    stage.open();
    Row row;
    EXPECT_EQ(stage.getNext(row), StageState::kEof);
    stage.close();
}

TEST(TopKStageTest, RandomizedResultsMatchSortAndLimitWithNullsTiesAndMixedNumerics) {
    auto table = std::make_shared<Table>();
    table->schema = Schema({"v"});

    std::mt19937 generator(42);
    std::uniform_int_distribution<int> values(-5, 5);
    for (std::uint64_t ordinal = 0; ordinal < 200; ++ordinal) {
        if (ordinal % 13 == 0) {
            table->rows.push_back(Row{ordinal, {nullptr}});
        } else if (ordinal % 2 == 0) {
            table->rows.push_back(Row{ordinal, {static_cast<std::int64_t>(values(generator))}});
        } else {
            table->rows.push_back(Row{ordinal, {static_cast<double>(values(generator))}});
        }
    }

    for (const SortDirection direction : {SortDirection::kAscending, SortDirection::kDescending}) {
        for (const std::uint64_t k : {0u, 1u, 3u, 25u, 500u}) {
            auto expected_scan = std::make_unique<CollectionScanStage>(table);
            auto expected_sort = std::make_unique<SortStage>(std::move(expected_scan), 0, direction);
            LimitStage expected_stage(std::move(expected_sort), k);
            expected_stage.open();
            const std::vector<Row> expected = drain(expected_stage);
            expected_stage.close();

            auto actual_scan = std::make_unique<CollectionScanStage>(table);
            TopKStage actual_stage(std::move(actual_scan), 0, direction, k);
            actual_stage.open();
            const std::vector<Row> actual = drain(actual_stage);
            actual_stage.close();

            ASSERT_EQ(actual.size(), expected.size()) << "k=" << k;
            for (std::size_t i = 0; i < actual.size(); ++i) {
                EXPECT_EQ(actual[i].ordinal, expected[i].ordinal) << "k=" << k << ", result index=" << i;
            }
        }
    }
}

class ThrowingGetNextStage final : public PlanStageBase {
public:
    explicit ThrowingGetNextStage(std::shared_ptr<bool> closed) : closed_(std::move(closed)) {}

    std::string stageName() const override { return "ThrowingGetNextStage"; }

protected:
    void onOpen() override {}

    StageState onGetNext(Row& /*output*/) override {
        throw QueryLumeError(ErrorCode::kIncompatibleTypes, "intentional test failure");
    }

    void onClose() noexcept override { *closed_ = true; }

private:
    std::shared_ptr<bool> closed_;
};

TEST(PlanStageExecutionGuardTest, ClosesStageWhenGetNextThrows) {
    auto closed = std::make_shared<bool>(false);
    ThrowingGetNextStage stage(closed);

    // Once construction succeeds, the guard owns cleanup for the rest of this
    // execution scope, even if getNext() terminates execution with an exception.
    {
        PlanStageExecutionGuard execution(stage);
        Row row;
        EXPECT_THROW(stage.getNext(row), QueryLumeError);
    }  // The guard's destructor closes the stage.

    EXPECT_TRUE(*closed);
}

class ThrowingOpenStage final : public PlanStageBase {
public:
    explicit ThrowingOpenStage(std::shared_ptr<bool> closed) : closed_(std::move(closed)) {}

    std::string stageName() const override { return "ThrowingOpenStage"; }

protected:
    void onOpen() override {
        throw QueryLumeError(ErrorCode::kIncompatibleTypes, "intentional open failure");
    }

    StageState onGetNext(Row& /*output*/) override { return StageState::kEof; }

    void onClose() noexcept override { *closed_ = true; }

private:
    std::shared_ptr<bool> closed_;
};

TEST(PlanStageExecutionGuardTest, FailedOpenRollsBackPartialStageState) {
    auto closed = std::make_shared<bool>(false);
    ThrowingOpenStage stage(closed);

    // A guard that throws while calling open() is never fully constructed, so
    // its destructor cannot run. PlanStageBase::open() must close any partially
    // opened stage tree before propagating the exception.
    EXPECT_THROW(stage.open(), QueryLumeError);
    EXPECT_TRUE(*closed);

    // Rollback leaves the stage closed rather than reusable or partially open.
    Row row;
    try {
        static_cast<void>(stage.getNext(row));
        FAIL() << "expected QueryLumeError";
    } catch (const QueryLumeError& error) {
        EXPECT_EQ(error.code(), ErrorCode::kInvalidStageLifecycle);
    }
}

}  // namespace
}  // namespace querylume
