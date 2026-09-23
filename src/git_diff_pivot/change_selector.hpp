#pragma once

#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

#include "git_diff_pivot/diff_document.hpp"
#include "git_diff_pivot/repeated_change_detector.hpp"

namespace git_diff_pivot {

// A changed line not covered by any accepted RepeatedSequence.
struct UniqueLine {
    std::string filePath;
    ChangedLine line;
    // Originating ChangeHunk index; distinguishes hunks within the same file.
    std::size_t hunkIndex{};
    // Copied from the hunk, for a git-style header even if this line's own kind is absent.
    std::uint32_t hunkAnchorOldLine{};
    std::uint32_t hunkAnchorNewLine{};
};

// Conflict-free accepted common changes, plus every uncovered changed line.
struct ChangeSelection {
    std::vector<RepeatedSequence> commonChanges;
    std::vector<UniqueLine> uniqueLines;
};

// Chooses a conflict-free, deterministic set of repeated sequences to show
// as common changes. Ranks candidates longest-first (ties by gain) so a
// long verbatim repeat stays whole instead of fragmenting around a shorter,
// more frequent sub-sequence. A conflicting candidate is split into smaller
// candidates over its still-free token range/occurrences rather than
// dropped. Everything left uncovered is exposed as a unique line.
class ChangeSelector {
public:
    // Candidates below these thresholds are discarded before ranking; both must be >= 1.
    explicit ChangeSelector(std::size_t minSequenceLength = 1, std::size_t minOccurrenceCount = 2)
        : minSequenceLength_(minSequenceLength), minOccurrenceCount_(minOccurrenceCount) {
        assert(minSequenceLength_ >= 1);
        assert(minOccurrenceCount_ >= 1);
    }

    [[nodiscard]] ChangeSelection Select(const DiffDocument& document, std::vector<RepeatedSequence> candidates) const;

private:
    std::size_t minSequenceLength_;
    std::size_t minOccurrenceCount_;
};

}  // namespace git_diff_pivot
