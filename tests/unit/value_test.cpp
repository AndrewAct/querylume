#include "querylume/data/value.h"

#include <gtest/gtest.h>

#include <limits>

#include "querylume/common/error.h"

namespace querylume {
namespace {

TEST(ValueTest, IntegerComparison) {
    EXPECT_EQ(compare(Value(int64_t{1}), Value(int64_t{2})), CompareResult::kLess);
    EXPECT_EQ(compare(Value(int64_t{2}), Value(int64_t{2})), CompareResult::kEqual);
    EXPECT_EQ(compare(Value(int64_t{3}), Value(int64_t{2})), CompareResult::kGreater);
}

TEST(ValueTest, DoubleComparison) {
    EXPECT_EQ(compare(Value(1.5), Value(2.5)), CompareResult::kLess);
    EXPECT_EQ(compare(Value(2.5), Value(2.5)), CompareResult::kEqual);
}

TEST(ValueTest, MixedIntDoubleComparison) {
    EXPECT_EQ(compare(Value(int64_t{2}), Value(2.0)), CompareResult::kEqual);
    EXPECT_EQ(compare(Value(int64_t{1}), Value(1.5)), CompareResult::kLess);
    EXPECT_EQ(compare(Value(2.5), Value(int64_t{2})), CompareResult::kGreater);
}

TEST(ValueTest, Int64ComparisonDoesNotLosePrecisionAboveDoubleExactRange) {
    constexpr std::int64_t kExactlyRepresentable = 9007199254740992LL;  // 2^53
    constexpr std::int64_t kNextInteger = kExactlyRepresentable + 1;

    EXPECT_EQ(compare(Value(kExactlyRepresentable), Value(kNextInteger)), CompareResult::kLess);
    EXPECT_FALSE(valuesEqual(Value(kExactlyRepresentable), Value(kNextInteger)));
    EXPECT_EQ(compare(Value(kNextInteger), Value(static_cast<double>(kExactlyRepresentable))),
              CompareResult::kGreater);
}

TEST(ValueTest, MixedNumericComparisonHandlesInt64Boundaries) {
    const auto minimum = std::numeric_limits<std::int64_t>::min();
    const auto maximum = std::numeric_limits<std::int64_t>::max();

    EXPECT_EQ(compare(Value(minimum), Value(-9223372036854775808.0)), CompareResult::kEqual);
    EXPECT_EQ(compare(Value(maximum), Value(9223372036854775808.0)), CompareResult::kLess);
    EXPECT_EQ(compare(Value(9223372036854775808.0), Value(maximum)), CompareResult::kGreater);
}

TEST(ValueTest, NaNComparisonIsRejected) {
    const Value nan = std::numeric_limits<double>::quiet_NaN();
    try {
        static_cast<void>(compare(nan, Value(1.0)));
        FAIL() << "expected QueryLumeError";
    } catch (const QueryLumeError& error) {
        EXPECT_EQ(error.code(), ErrorCode::kUnsupportedValueType);
        EXPECT_NE(std::string(error.what()).find("NaN"), std::string::npos);
    }
}

TEST(ValueTest, StringComparison) {
    EXPECT_EQ(compare(Value(std::string("a")), Value(std::string("b"))), CompareResult::kLess);
    EXPECT_EQ(compare(Value(std::string("b")), Value(std::string("b"))), CompareResult::kEqual);
}

TEST(ValueTest, NullEqualityIsTrue) { EXPECT_TRUE(valuesEqual(Value(nullptr), Value(nullptr))); }

TEST(ValueTest, NullVsNonNullEqualityIsFalse) {
    EXPECT_FALSE(valuesEqual(Value(nullptr), Value(int64_t{1})));
    EXPECT_FALSE(valuesEqual(Value(int64_t{1}), Value(nullptr)));
}

TEST(ValueTest, OrderedComparisonWithNullIsUnordered) {
    EXPECT_EQ(compare(Value(nullptr), Value(int64_t{1})), CompareResult::kUnordered);
    EXPECT_EQ(compare(Value(int64_t{1}), Value(nullptr)), CompareResult::kUnordered);
    EXPECT_EQ(compare(Value(nullptr), Value(nullptr)), CompareResult::kUnordered);
}

TEST(ValueTest, BooleanEquality) {
    EXPECT_TRUE(valuesEqual(Value(true), Value(true)));
    EXPECT_FALSE(valuesEqual(Value(true), Value(false)));
}

TEST(ValueTest, BooleanOrderedComparisonThrows) {
    EXPECT_THROW(compare(Value(true), Value(false)), QueryLumeError);
}

TEST(ValueTest, IncompatibleTypesThrow) {
    try {
        static_cast<void>(compare(Value(std::string("x")), Value(std::int64_t{1})));
        FAIL() << "expected QueryLumeError";
    } catch (const QueryLumeError& error) {
        EXPECT_EQ(error.code(), ErrorCode::kIncompatibleTypes);
        EXPECT_NE(std::string(error.what()).find("string and int64"), std::string::npos);
    }
}

}  // namespace
}  // namespace querylume
