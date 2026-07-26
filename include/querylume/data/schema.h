#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace querylume {

// A schema-backed column layout shared by all rows in a Table. Resolving a
// field name to a column index happens once (in the binder); execution reads
// use the resolved index so no stage performs field-name lookups per row.
class Schema {
public:
    Schema() = default;
    explicit Schema(std::vector<std::string> fields);

    const std::vector<std::string>& fields() const noexcept { return fields_; }
    std::size_t columnCount() const noexcept { return fields_.size(); }

    std::optional<std::size_t> indexOf(const std::string& field) const;

private:
    std::vector<std::string> fields_;
    std::unordered_map<std::string, std::size_t> index_by_name_;
};

}  // namespace querylume
