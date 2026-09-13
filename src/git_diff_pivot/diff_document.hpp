#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "git_diff_pivot/token_interner.hpp"

namespace git_diff_pivot {

// A changed line is either an addition or a removal.
enum class DiffLineKind : std::uint8_t {
    Added,
    Removed,
};

// One normalized, tokenized changed line, together with enough information
// to map it back to its place in the original diff.
struct ChangedLine {
    TokenId token{};
    std::uint32_t lineNumber{};  // 1-based line number in the file/diff.
    DiffLineKind kind{DiffLineKind::Added};
};

// A "hunk": a maximal, continuous run of changed lines belonging to a single
// file. Gaps between independent changed regions never occur inside a hunk,
// which is what allows the future repeated-sequence search to treat a hunk
// boundary as a hard stop.
struct ChangeHunk {
    std::string filePath;
    std::vector<ChangedLine> lines;
    // Old/new file line number immediately before this hunk's first line,
    // i.e. the position a git-style "@@ -a,b +c,d @@" header would anchor on.
    std::uint32_t anchorOldLine{};
    std::uint32_t anchorNewLine{};
};

// Represents a full multi-file diff as a set of hunks (see ChangeHunk). This
// model must retain enough information to reconstruct a compact,
// review-oriented rendering: which files/locations a repeated sequence of
// changes came from.
class DiffDocument {
public:
    void AddHunk(ChangeHunk hunk);

    [[nodiscard]] const std::vector<ChangeHunk>& Hunks() const noexcept { return hunks_; }
    [[nodiscard]] std::size_t HunkCount() const noexcept { return hunks_.size(); }

private:
    std::vector<ChangeHunk> hunks_;
};

}  // namespace git_diff_pivot
