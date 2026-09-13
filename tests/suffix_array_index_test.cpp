#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "git_diff_pivot/suffix_array_index.hpp"

using git_diff_pivot::SuffixArrayIndex;

TEST_CASE("SuffixArrayIndex builds a valid suffix array for a simple token stream", "[suffix-array]") {
    // Tokens for "banana" mapped to small integers: b=0, a=1, n=2.
    const std::vector<std::int32_t> tokens{0, 1, 2, 1, 2, 1};

    SuffixArrayIndex index;
    index.Build(tokens);

    const auto& suffixArray = index.SuffixArray();
    REQUIRE(suffixArray.size() == tokens.size());

    // A valid suffix array is a permutation of [0, n).
    std::vector<std::int32_t> sorted(suffixArray.begin(), suffixArray.end());
    std::sort(sorted.begin(), sorted.end());
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        REQUIRE(sorted[i] == static_cast<std::int32_t>(i));
    }

    REQUIRE(index.LcpArray().size() == tokens.size());
}

TEST_CASE("SuffixArrayIndex handles an empty token stream", "[suffix-array]") {
    SuffixArrayIndex index;
    index.Build({});
    REQUIRE(index.SuffixArray().empty());
    REQUIRE(index.LcpArray().empty());
}
