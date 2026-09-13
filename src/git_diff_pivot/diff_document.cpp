#include "git_diff_pivot/diff_document.hpp"

#include <utility>

namespace git_diff_pivot {

void DiffDocument::AddHunk(ChangeHunk hunk) {
    hunks_.push_back(std::move(hunk));
}

}  // namespace git_diff_pivot
