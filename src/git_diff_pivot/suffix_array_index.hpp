#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace git_diff_pivot {

// Thin abstraction around the libsais suffix-array/LCP construction library.
// No libsais types or headers are exposed here; callers only ever see plain
// integer vectors. This keeps the third-party dependency isolated to the
// single translation unit that implements this class.
//
// The current implementation builds a suffix array (and LCP array) for a
// single token stream. Building a *generalized* suffix array across multiple
// documents (using separator tokens) and using the LCP array to find
// repeated sequences is future work.
class SuffixArrayIndex {
public:
    // Builds the suffix array (and LCP array) for `tokens`.
    void Build(std::span<const std::int32_t> tokens);

    [[nodiscard]] const std::vector<std::int32_t>& SuffixArray() const noexcept { return suffixArray_; }
    [[nodiscard]] const std::vector<std::int32_t>& LcpArray() const noexcept { return lcpArray_; }

private:
    std::vector<std::int32_t> suffixArray_;
    std::vector<std::int32_t> lcpArray_;
};

}  // namespace git_diff_pivot
