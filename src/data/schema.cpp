#include "querylume/data/schema.h"

namespace querylume {

Schema::Schema(std::vector<std::string> fields) : fields_(std::move(fields)) {
    for (std::size_t i = 0; i < fields_.size(); ++i) {
        index_by_name_.emplace(fields_[i], i);
    }
}

std::optional<std::size_t> Schema::indexOf(const std::string& field) const {
    auto it = index_by_name_.find(field);
    if (it == index_by_name_.end()) return std::nullopt;
    return it->second;
}

}  // namespace querylume
