#include "querylume/data/value.h"

#include <gtest/gtest.h>

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
    EXPECT_THROW(valuesEqual(Value(std::string("x")), Value(int64_t{1})), QueryLumeError);
    EXPECT_THROW(compare(Value(std::string("x")), Value(int64_t{1})), QueryLumeError);
}

}  // namespace
}  // namespace querylume
