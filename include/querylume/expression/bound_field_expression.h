#pragma once

#include <cstddef>
#include <string>

#include "querylume/expression/expression.h"

namespace querylume {

// A field reference resolved to a fixed column index by the binder. Reading
// a row's value is a direct vector index, never a name lookup.
class BoundFieldExpression final : public Expression {
public:
    BoundFieldExpression(std::size_t column_index, std::string field_name)
        : column_index_(column_index), field_name_(std::move(field_name)) {}

    Value evaluate(const Row& row) const override { return row.values.at(column_index_); }

    std::string describe() const override { return field_name_; }

    std::size_t columnIndex() const noexcept { return column_index_; }
    const std::string& fieldName() const noexcept { return field_name_; }

private:
    std::size_t column_index_;
    std::string field_name_;
};

}  // namespace querylume
