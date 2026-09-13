#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "git_diff_pivot/diff_document.hpp"
#include "git_diff_pivot/suffix_array_index.hpp"

namespace git_diff_pivot {

// Identifies one token position inside a DiffDocument by which hunk it came
// from and its offset within that hunk's line list.
struct HunkTokenPosition {
    std::size_t hunkIndex{};
    std::size_t offsetWithinHunk{};
};

// Builds one generalized suffix array/LCP view over every hunk in a
// DiffDocument. Hunks are concatenated with unique separator tokens, each
// greater than every real token id and distinct from every other hunk's
// separator, so a common prefix between two suffixes can never extend past
// a hunk boundary.
//
// This wraps the single-stream SuffixArrayIndex rather than duplicating its
// libsais-facing logic; libsais itself stays isolated to
// suffix_array_index.cpp.
class GeneralizedSuffixArrayIndex {
public:
    // Builds the generalized suffix array/LCP arrays for every hunk in `document`.
    void Build(const DiffDocument& document);

    [[nodiscard]] const std::vector<std::int32_t>& SuffixArray() const noexcept { return index_.SuffixArray(); }
    [[nodiscard]] const std::vector<std::int32_t>& LcpArray() const noexcept { return index_.LcpArray(); }

    // Maps a position in the concatenated token stream back to the hunk and
    // offset it came from. Valid for every position in [0, SuffixArray().size()).
    [[nodiscard]] const HunkTokenPosition& PositionFor(std::size_t position) const;

    // True when `position` refers to a synthetic per-hunk separator token
    // rather than a real diff-line token.
    [[nodiscard]] bool IsSeparator(std::size_t position) const;

private:
    SuffixArrayIndex index_;
    std::vector<HunkTokenPosition> positions_;
    std::vector<bool> isSeparator_;
};

}  // namespace git_diff_pivot
