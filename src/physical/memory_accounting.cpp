#include "querylume/physical/memory_accounting.h"

#include "querylume/data/value.h"

namespace querylume {

namespace {
std::uint64_t approximateValueBytes(const Value& value) {
    if (std::holds_alternative<std::string>(value)) {
        return sizeof(Value) + std::get<std::string>(value).size();
    }
    return sizeof(Value);
}
}  // namespace

std::uint64_t approximateRowBytes(const Row& row) {
    std::uint64_t bytes = sizeof(Row);
    for (const Value& value : row.values) {
        bytes += approximateValueBytes(value);
    }
    return bytes;
}

std::uint64_t approximateRowsBytes(const std::vector<Row>& rows) {
    std::uint64_t bytes = 0;
    for (const Row& row : rows) {
        bytes += approximateRowBytes(row);
    }
    return bytes;
}

}  // namespace querylume
