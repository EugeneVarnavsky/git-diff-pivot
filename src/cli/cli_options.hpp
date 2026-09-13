#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "git_diff_pivot/output_renderer.hpp"

namespace git_diff_pivot {

constexpr std::size_t kDefaultMinSequenceLength = 1;
constexpr std::size_t kDefaultMinOccurrenceCount = 2;

// Parsed git-diff-pivot CLI options
struct CliOptions {
    bool helpRequested = false;
    bool versionRequested = false;
    std::optional<std::string> inputPath;
    std::optional<std::string> outputPath;
    std::size_t minSequenceLength = kDefaultMinSequenceLength;
    std::size_t minOccurrenceCount = kDefaultMinOccurrenceCount;
    OutputFormat outputFormat = OutputFormat::Text;
};

// Parses git-diff-pivot CLI arguments (`args` excludes argv[0]) into a CliOptions
// struct. Flag values may follow as a separate argument or via `--flag=value`.
// When --output is given without --output-type, the format is inferred from
// the --output path's extension (matching ParseOutputFormat's txt/md/json
// values); an unrecognized extension is an error. When --output-type is given
// without --output, it only selects the stdout rendering format. Throws
// std::invalid_argument describing the first invalid argument encountered.
[[nodiscard]] CliOptions ParseCliOptions(const std::vector<std::string>& args);

}  // namespace git_diff_pivot
