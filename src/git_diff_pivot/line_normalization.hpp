#pragma once

#include <string>
#include <string_view>

namespace git_diff_pivot {

// Produces the canonical representation of a changed diff line used for
// token comparison. Two diff lines that differ only in insignificant
// whitespace normalize to the same value.
//
// The current normalization is intentionally minimal:
//   - leading and trailing whitespace is trimmed;
//   - runs of internal whitespace are collapsed to a single space.
// It does not understand language syntax or diff markers.
[[nodiscard]] std::string NormalizeDiffLine(std::string_view line);

}  // namespace git_diff_pivot
