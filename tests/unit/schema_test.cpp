#include "querylume/data/schema.h"

#include <gtest/gtest.h>

namespace querylume {
namespace {

TEST(SchemaTest, PreservesFieldOrder) {
    Schema schema({"symbol", "price", "volume"});
    EXPECT_EQ(schema.fields(), (std::vector<std::string>{"symbol", "price", "volume"}));
}

TEST(SchemaTest, IndexOfKnownField) {
    Schema schema({"symbol", "price"});
    ASSERT_TRUE(schema.indexOf("price").has_value());
    EXPECT_EQ(schema.indexOf("price").value(), 1u);
}

TEST(SchemaTest, IndexOfUnknownFieldIsNullopt) {
    Schema schema({"symbol"});
    EXPECT_FALSE(schema.indexOf("missing").has_value());
}

}  // namespace
}  // namespace querylume
