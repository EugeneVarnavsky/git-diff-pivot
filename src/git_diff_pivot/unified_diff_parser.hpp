#pragma once

#include <istream>

#include "git_diff_pivot/diff_document.hpp"
#include "git_diff_pivot/token_interner.hpp"

namespace git_diff_pivot {

// Parses real unified Git diff text (as produced by `git diff`) into a
// DiffDocument. Recognizes "diff --git" file boundaries, "---"/"+++" path
// headers, and "@@" hunk headers; treats context lines and hunk/file
// boundaries as hunk boundaries; and skips binary diff entries.
//
// Reads the stream one line at a time so the whole diff never needs to be
// materialized in memory before parsing.
class UnifiedDiffParser {
public:
    [[nodiscard]] static DiffDocument Parse(std::istream& input, TokenInterner& interner);
};

}  // namespace git_diff_pivot
