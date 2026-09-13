#pragma once

#include <filesystem>
#include <istream>
#include <string>
#include <vector>

namespace git_diff_pivot {

// Reads raw diff text one line at a time from a stream or file. It knows
// nothing about Git; UnifiedDiffParser is responsible for that.
class DiffInputReader {
public:
    [[nodiscard]] static std::vector<std::string> ReadLines(std::istream& input);
    [[nodiscard]] static std::vector<std::string> ReadLinesFromFile(const std::filesystem::path& path);
};

}  // namespace git_diff_pivot
