#include "querylume/physical/row_comparator.h"

#include "querylume/data/value.h"

namespace querylume {

bool RowComparator::operator()(const Row& a, const Row& b) const {
    const Value& va = a.values[column_index_];
    const Value& vb = b.values[column_index_];
    const bool a_null = std::holds_alternative<std::nullptr_t>(va);
    const bool b_null = std::holds_alternative<std::nullptr_t>(vb);

    if (a_null && b_null) {
        return a.ordinal < b.ordinal;
    }
    if (a_null != b_null) {
        // Exactly one side is null: ascending places non-null before null,
        // descending places null before non-null.
        return direction_ == SortDirection::kAscending ? (!a_null && b_null) : (a_null && !b_null);
    }

    const CompareResult r = compare(va, vb);
    if (r == CompareResult::kEqual) {
        return a.ordinal < b.ordinal;
    }
    if (direction_ == SortDirection::kAscending) {
        return r == CompareResult::kLess;
    }
    return r == CompareResult::kGreater;
}

}  // namespace querylume
