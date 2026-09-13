#include "git_diff_pivot/generalized_suffix_array_index.hpp"

#include <algorithm>

namespace git_diff_pivot {

void GeneralizedSuffixArrayIndex::Build(const DiffDocument& document) {
    const auto& hunks = document.Hunks();

    TokenId maxRealToken = 0;
    bool hasRealToken = false;
    for (const auto& hunk : hunks) {
        for (const auto& line : hunk.lines) {
            maxRealToken = hasRealToken ? std::max(maxRealToken, line.token) : line.token;
            hasRealToken = true;
        }
    }

    // Every hunk gets its own separator token, greater than every real token
    // and distinct from every other hunk's separator.
    const auto firstSeparator = static_cast<std::int64_t>(hasRealToken ? maxRealToken + 1 : 0);

    std::vector<std::int32_t> stream;
    positions_.clear();
    isSeparator_.clear();

    for (std::size_t hunkIndex = 0; hunkIndex < hunks.size(); ++hunkIndex) {
        const auto& hunk = hunks[hunkIndex];
        for (std::size_t offset = 0; offset < hunk.lines.size(); ++offset) {
            stream.push_back(static_cast<std::int32_t>(hunk.lines[offset].token));
            positions_.push_back(HunkTokenPosition{.hunkIndex = hunkIndex, .offsetWithinHunk = offset});
            isSeparator_.push_back(false);
        }
        stream.push_back(static_cast<std::int32_t>(firstSeparator + static_cast<std::int64_t>(hunkIndex)));
        positions_.push_back(HunkTokenPosition{.hunkIndex = hunkIndex, .offsetWithinHunk = hunk.lines.size()});
        isSeparator_.push_back(true);
    }

    index_.Build(stream);
}

const HunkTokenPosition& GeneralizedSuffixArrayIndex::PositionFor(std::size_t position) const {
    return positions_.at(position);
}

bool GeneralizedSuffixArrayIndex::IsSeparator(std::size_t position) const {
    return isSeparator_.at(position);
}

}  // namespace git_diff_pivot
