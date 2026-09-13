#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "git_diff_pivot/diff_document.hpp"
#include "git_diff_pivot/repeated_change_detector.hpp"

namespace git_diff_pivot {

// One changed line left over after selection: not part of any accepted
// RepeatedSequence, so the renderer must show it under its own file.
struct UniqueLine {
    std::string filePath;
    ChangedLine line;
    // Index of the originating ChangeHunk (DiffDocument::Hunks()), so the
    // renderer can tell apart lines from different original hunks even
    // when they belong to the same file.
    std::size_t hunkIndex{};
    // Copied from the originating ChangeHunk, for building a git-style hunk
    // header even when this line's own kind has no line in the same run.
    std::uint32_t hunkAnchorOldLine{};
    std::uint32_t hunkAnchorNewLine{};
};

// The result of change selection: the conflict-free set of accepted common
// changes, plus every changed line not covered by any of them.
struct ChangeSelection {
    std::vector<RepeatedSequence> commonChanges;
    std::vector<UniqueLine> uniqueLines;
};

// Chooses a conflict-free, deterministic set of repeated sequences worth
// extracting into the compact review representation.
// Candidates are ranked by estimated review-effort gain and accepted
// greedily as long as none of their occurrences overlap an already-claimed
// hunk range. Every changed line not covered by an accepted candidate is
// exposed separately so the renderer can still show it under its file.
class ChangeSelector {
public:
    // Candidates shorter than `minSequenceLength` tokens or with fewer than
    // `minOccurrenceCount` occurrences are discarded before ranking.
    explicit ChangeSelector(std::size_t minSequenceLength = 1, std::size_t minOccurrenceCount = 2)
        : minSequenceLength_(minSequenceLength), minOccurrenceCount_(minOccurrenceCount) {}

    [[nodiscard]] ChangeSelection Select(const DiffDocument& document, std::vector<RepeatedSequence> candidates) const;

private:
    std::size_t minSequenceLength_;
    std::size_t minOccurrenceCount_;
};

}  // namespace git_diff_pivot
