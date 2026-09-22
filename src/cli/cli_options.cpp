#include "cli/cli_options.hpp"

#include <filesystem>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace git_diff_pivot {

namespace {

// Parses a positive integer CLI argument value, throwing std::invalid_argument
// with a message identifying the offending flag on failure.
std::size_t ParseThreshold(std::string_view flag, std::string_view value) {
    try {
        std::size_t parsedChars = 0;
        const auto parsed = std::stoul(std::string(value), &parsedChars);
        if (parsedChars != value.size() || parsed == 0) {
            throw std::invalid_argument("");
        }
        return parsed;
    } catch (const std::exception&) {
        throw std::invalid_argument("invalid value for " + std::string(flag) + ": '" + std::string(value) + "'");
    }
}

// Splits "--flag=value" into ("--flag", "value"); the value half is
// std::nullopt when `arg` contains no '='.
std::pair<std::string_view, std::optional<std::string_view>> SplitFlagValue(std::string_view arg) {
    const auto eqPos = arg.find('=');
    if (eqPos == std::string_view::npos) {
        return {arg, std::nullopt};
    }
    return {arg.substr(0, eqPos), arg.substr(eqPos + 1)};
}

// Infers an OutputFormat from an --output path's extension (e.g. "out.md" ->
// Markdown), reusing ParseOutputFormat's txt/md/json values. Returns
// std::nullopt when the extension doesn't match a known format.
std::optional<OutputFormat> InferOutputFormatFromExtension(const std::string& outputPath) {
    std::string extension = std::filesystem::path(outputPath).extension().string();
    if (!extension.empty() && extension.front() == '.') {
        extension.erase(0, 1);
    }
    return ParseOutputFormat(extension);
}

}  // namespace

CliOptions ParseCliOptions(const std::vector<std::string>& args) {
    CliOptions options;
    bool outputTypeExplicit = false;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        if (arg == "-h" || arg == "--help") {
            options.helpRequested = true;
            return options;
        }
        if (arg == "-v" || arg == "--version") {
            options.versionRequested = true;
            return options;
        }
        if (arg == "--txt" || arg == "--md" || arg == "--json") {
            options.outputFormat = *ParseOutputFormat(arg.substr(2));
            outputTypeExplicit = true;
            continue;
        }

        const auto [flag, inlineValue] = SplitFlagValue(arg);
        const bool needsValue =
            flag == "--min-length" || flag == "--min-occurrences" || flag == "--output" || flag == "--output-type";
        if (!needsValue) {
            options.inputPath = std::string(arg);
            continue;
        }

        std::string_view value;
        if (inlineValue) {
            value = *inlineValue;
        } else {
            if (i + 1 >= args.size()) {
                throw std::invalid_argument("missing value for " + std::string(flag));
            }
            value = args[++i];
        }

        if (flag == "--min-length") {
            options.minSequenceLength = ParseThreshold(flag, value);
        } else if (flag == "--min-occurrences") {
            options.minOccurrenceCount = ParseThreshold(flag, value);
        } else if (flag == "--output") {
            options.outputPath = std::string(value);
        } else if (flag == "--output-type") {
            const auto parsed = ParseOutputFormat(value);
            if (!parsed) {
                throw std::invalid_argument("invalid value for --output-type: '" + std::string(value) + "'");
            }
            options.outputFormat = *parsed;
            outputTypeExplicit = true;
        }
    }

    if (!outputTypeExplicit && options.outputPath) {
        const auto inferred = InferOutputFormatFromExtension(*options.outputPath);
        if (!inferred) {
            throw std::invalid_argument("cannot infer --output-type from --output file extension: '" +
                                         *options.outputPath + "'");
        }
        options.outputFormat = *inferred;
    }

    return options;
}

}  // namespace git_diff_pivot
