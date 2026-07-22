#include <CLI/CLI.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>

#include "querylume/common/error.h"
#include "querylume/data/json_loader.h"
#include "querylume/explain/json_serialize.h"
#include "querylume/explain/query_facade.h"

namespace {

nlohmann::json loadJsonFile(const std::string& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw querylume::QueryLumeError(querylume::ErrorCode::kJsonParseError,
                                        "failed to open file: " + path);
    }
    nlohmann::json parsed;
    try {
        stream >> parsed;
    } catch (const nlohmann::json::parse_error& e) {
        throw querylume::QueryLumeError(querylume::ErrorCode::kJsonParseError,
                                        "failed to parse JSON in '" + path + "': " + e.what());
    }
    return parsed;
}

struct CommonArgs {
    std::string data_path;
    std::string pipeline_path;
};

void addCommonOptions(CLI::App* command, CommonArgs& args) {
    command->add_option("--data", args.data_path, "Path to input data JSON file")->required();
    command->add_option("--pipeline", args.pipeline_path, "Path to pipeline JSON file")->required();
}

int runCommand(const CommonArgs& args) {
    auto table = std::make_shared<querylume::Table>(querylume::loadTableFromJsonFile(args.data_path));
    nlohmann::json pipeline_json = loadJsonFile(args.pipeline_path);

    querylume::QueryResult result = querylume::runPipeline(table, pipeline_json);

    nlohmann::json output = nlohmann::json::array();
    for (const auto& row : result.rows) {
        output.push_back(querylume::toJson(row, result.output_schema));
    }
    std::cout << output.dump(2) << "\n";
    return 0;
}

int explainCommand(const CommonArgs& args) {
    auto table = std::make_shared<querylume::Table>(querylume::loadTableFromJsonFile(args.data_path));
    nlohmann::json pipeline_json = loadJsonFile(args.pipeline_path);

    nlohmann::json output = querylume::explainPipeline(table, pipeline_json);
    std::cout << output.dump(2) << "\n";
    return 0;
}

int explainAnalyzeCommand(const CommonArgs& args, bool include_results) {
    auto table = std::make_shared<querylume::Table>(querylume::loadTableFromJsonFile(args.data_path));
    nlohmann::json pipeline_json = loadJsonFile(args.pipeline_path);

    nlohmann::json output = querylume::explainAnalyzePipeline(table, pipeline_json, include_results);
    std::cout << output.dump(2) << "\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        CLI::App app{"querylume: an embeddable single-process query execution and diagnostics engine"};
        app.require_subcommand(1);

        CommonArgs run_args;
        CLI::App* run_cmd =
            app.add_subcommand("run", "Execute a pipeline and print the resulting rows as JSON");
        addCommonOptions(run_cmd, run_args);

        CommonArgs explain_args;
        CLI::App* explain_cmd = app.add_subcommand(
            "explain", "Print the parsed/logical/optimized/physical plan without executing it");
        addCommonOptions(explain_cmd, explain_args);

        CommonArgs analyze_args;
        bool include_results = false;
        CLI::App* analyze_cmd = app.add_subcommand(
            "explain-analyze", "Execute the pipeline and print the plan with runtime statistics");
        addCommonOptions(analyze_cmd, analyze_args);
        analyze_cmd->add_flag("--with-result", include_results,
                              "Include the query result rows in the output");

        CLI11_PARSE(app, argc, argv);

        if (run_cmd->parsed()) return runCommand(run_args);
        if (explain_cmd->parsed()) return explainCommand(explain_args);
        if (analyze_cmd->parsed()) return explainAnalyzeCommand(analyze_args, include_results);
    } catch (const querylume::QueryLumeError& e) {
        std::cerr << "error [" << querylume::toString(e.code()) << "]: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "error: unknown failure\n";
        return 1;
    }

    return 1;
}
