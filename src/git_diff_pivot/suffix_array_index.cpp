#include "git_diff_pivot/suffix_array_index.hpp"

#include <algorithm>
#include <stdexcept>

#include <libsais.h>

namespace git_diff_pivot {

void SuffixArrayIndex::Build(std::span<const std::int32_t> tokens) {
    const auto length = static_cast<std::int32_t>(tokens.size());

    suffixArray_.assign(static_cast<std::size_t>(length), 0);
    lcpArray_.assign(static_cast<std::size_t>(length), 0);

    if (length == 0) {
        return;
    }

    // libsais_int mutates its input buffer (restoring it afterwards), so work
    // on a private copy instead of the caller's token stream.
    std::vector<std::int32_t> mutableTokens(tokens.begin(), tokens.end());
    const std::int32_t alphabetSize = *std::max_element(tokens.begin(), tokens.end()) + 1;

    if (libsais_int(mutableTokens.data(), suffixArray_.data(), length, alphabetSize, 0) != 0) {
        throw std::runtime_error("libsais_int failed to build the suffix array");
    }

    std::vector<std::int32_t> permutedLcp(static_cast<std::size_t>(length), 0);
    if (libsais_plcp_int(tokens.data(), suffixArray_.data(), permutedLcp.data(), length) != 0) {
        throw std::runtime_error("libsais_plcp_int failed to build the PLCP array");
    }
    if (libsais_lcp(permutedLcp.data(), suffixArray_.data(), lcpArray_.data(), length) != 0) {
        throw std::runtime_error("libsais_lcp failed to build the LCP array");
    }
}

}  // namespace git_diff_pivot
