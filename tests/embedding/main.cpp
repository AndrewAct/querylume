#include <memory>

#include "querylume/data/json_loader.h"
#include "querylume/explain/query_facade.h"

int main() {
    auto table = std::make_shared<querylume::Table>(querylume::loadTableFromJsonText(R"([{"value": 1}])"));
    const auto pipeline = nlohmann::json::parse(R"([{"$limit": 1}])");
    const querylume::QueryResult result = querylume::runPipeline(table, pipeline);
    return result.rows.size() == 1 ? 0 : 1;
}
