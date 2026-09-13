#pragma once

#include <string>
#include <vector>

#include "git_diff_pivot/diff_document.hpp"
#include "git_diff_pivot/token_interner.hpp"

namespace git_diff_pivot {

// Parses real unified Git diff text (as produced by `git diff`) into a
// DiffDocument. Recognizes "diff --git" file boundaries, "---"/"+++" path
// headers, and "@@" hunk headers; treats context lines and hunk/file
// boundaries as hunk boundaries; and skips binary diff entries.
class UnifiedDiffParser {
public:
    [[nodiscard]] static DiffDocument Parse(const std::vector<std::string>& lines, TokenInterner& interner);
};

}  // namespace git_diff_pivot
