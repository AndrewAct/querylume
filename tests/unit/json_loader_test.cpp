#include "querylume/data/json_loader.h"

#include <gtest/gtest.h>

#include "querylume/common/error.h"

namespace querylume {
namespace {

TEST(JsonLoaderTest, InfersDeterministicSchema) {
    Table table = loadTableFromJsonText(R"([{"a":1,"b":2},{"a":3,"b":4}])");
    EXPECT_EQ(table.schema.fields(), (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(table.rows.size(), 2u);
    EXPECT_EQ(table.rows[0].ordinal, 0u);
    EXPECT_EQ(table.rows[1].ordinal, 1u);
}

TEST(JsonLoaderTest, MissingFieldsBecomeNull) {
    Table table = loadTableFromJsonText(R"([{"a":1},{"b":2}])");
    EXPECT_EQ(table.schema.fields(), (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(table.rows[0].values[1], Value(nullptr));
    EXPECT_EQ(table.rows[1].values[0], Value(nullptr));
}

TEST(JsonLoaderTest, RejectsNestedObject) {
    EXPECT_THROW(loadTableFromJsonText(R"([{"a":{"nested":1}}])"), QueryLumeError);
}

TEST(JsonLoaderTest, RejectsArrayValue) {
    EXPECT_THROW(loadTableFromJsonText(R"([{"a":[1,2,3]}])"), QueryLumeError);
}

TEST(JsonLoaderTest, MixedIntegerAndDoubleValues) {
    Table table = loadTableFromJsonText(R"([{"a":1},{"a":1.5}])");
    EXPECT_EQ(table.rows[0].values[0], Value(int64_t{1}));
    EXPECT_EQ(table.rows[1].values[0], Value(1.5));
}

TEST(JsonLoaderTest, RejectsNonArrayTopLevel) {
    EXPECT_THROW(loadTableFromJsonText(R"({"a":1})"), QueryLumeError);
}

TEST(JsonLoaderTest, RejectsMalformedJson) {
    EXPECT_THROW(loadTableFromJsonText("not json"), QueryLumeError);
}

}  // namespace
}  // namespace querylume
