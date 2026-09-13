#include "git_diff_pivot/line_normalization.hpp"

#include <cctype>

namespace git_diff_pivot {

namespace {

bool IsSpace(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

}  // namespace

std::string NormalizeDiffLine(std::string_view line) {
    std::size_t begin = 0;
    std::size_t end = line.size();
    while (begin < end && IsSpace(line[begin])) {
        ++begin;
    }
    while (end > begin && IsSpace(line[end - 1])) {
        --end;
    }

    std::string result;
    result.reserve(end - begin);

    bool previousWasSpace = false;
    for (std::size_t i = begin; i < end; ++i) {
        const char c = line[i];
        if (IsSpace(c)) {
            if (!previousWasSpace) {
                result.push_back(' ');
            }
            previousWasSpace = true;
        } else {
            result.push_back(c);
            previousWasSpace = false;
        }
    }

    return result;
}

}  // namespace git_diff_pivot
