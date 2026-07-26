#pragma once

#include <vector>

#include "querylume/data/row.h"
#include "querylume/data/schema.h"

namespace querylume {

struct Table {
    Schema schema;
    std::vector<Row> rows;
};

}  // namespace querylume
