#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "git_diff_pivot/token_interner.hpp"
#include "git_diff_pivot/diff_document.hpp"

namespace git_diff_pivot {

// One occurrence of a repeated token sequence within a single file's hunk.
struct RepeatedSequenceOccurrence {
    std::string filePath;
    std::uint32_t startLine{};  // Line number of the first token in the occurrence.
    std::uint32_t length{};     // Number of tokens in the occurrence.
    // Precise (hunk, offset) address, used by ChangeSelector to claim exact
    // token ranges without relying on line-number arithmetic (filePath and
    // startLine alone cannot distinguish overlapping old/new line numbering).
    std::size_t hunkIndex{};
    std::size_t offsetWithinHunk{};
};

// A token sequence that occurs more than once across the diff, together with
// every place it occurs.
struct RepeatedSequence {
    std::vector<TokenId> tokens;
    std::vector<RepeatedSequenceOccurrence> occurrences;
};

// Finds repeated sequences of diff-line tokens across a DiffDocument using
// the generalized suffix array and LCP information from
// GeneralizedSuffixArrayIndex. Candidates are maximal: a candidate is never
// reported if every one of its occurrences could be extended by one more
// matching token without changing the occurrence count, since that longer
// extension is reported instead. Candidates may still overlap each other
// (e.g. "A B C" and its suffix "B C" at the same positions); resolving
// overlaps is the job of ChangeSelector, not this detector.
class RepeatedChangeDetector {
public:
    // `minSequenceLength` discards candidates shorter than this many tokens.
    explicit RepeatedChangeDetector(std::size_t minSequenceLength = 1) : minSequenceLength_(minSequenceLength) {}

    [[nodiscard]] std::vector<RepeatedSequence> Detect(const DiffDocument& document) const;

private:
    std::size_t minSequenceLength_;
};

}  // namespace git_diff_pivot
