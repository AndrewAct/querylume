#pragma once

namespace querylume {

// Sort direction is query semantics shared by the logical and physical
// layers. It lives in common/ so the logical plan never needs to depend on
// a physical execution implementation merely to describe ascending/descending.
enum class SortDirection {
    kAscending,
    kDescending,
};

}  // namespace querylume
