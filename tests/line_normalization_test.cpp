#include <catch2/catch_test_macros.hpp>

#include "git_diff_pivot/line_normalization.hpp"

using git_diff_pivot::NormalizeDiffLine;

TEST_CASE("NormalizeDiffLine trims leading and trailing whitespace", "[normalization]") {
    REQUIRE(NormalizeDiffLine("   hello world   ") == "hello world");
}

TEST_CASE("NormalizeDiffLine collapses internal whitespace runs", "[normalization]") {
    REQUIRE(NormalizeDiffLine("a    b\tc") == "a b c");
}

TEST_CASE("NormalizeDiffLine leaves already-normalized lines unchanged", "[normalization]") {
    REQUIRE(NormalizeDiffLine("int x = 1;") == "int x = 1;");
}

TEST_CASE("NormalizeDiffLine handles empty and whitespace-only input", "[normalization]") {
    REQUIRE(NormalizeDiffLine("").empty());
    REQUIRE(NormalizeDiffLine("   \t  ").empty());
}
