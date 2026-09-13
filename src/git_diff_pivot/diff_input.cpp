#include "git_diff_pivot/diff_input.hpp"

#include <fstream>
#include <stdexcept>

namespace git_diff_pivot {

std::vector<std::string> DiffInputReader::ReadLines(std::istream& input) {
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(std::move(line));
    }
    return lines;
}

std::vector<std::string> DiffInputReader::ReadLinesFromFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::in);
    if (!file) {
        throw std::runtime_error("Could not open diff input file: " + path.string());
    }
    return ReadLines(file);
}

}  // namespace git_diff_pivot
