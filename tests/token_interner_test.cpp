#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "git_diff_pivot/token_interner.hpp"

using git_diff_pivot::TokenInterner;

TEST_CASE("TokenInterner assigns the same id to identical text", "[interner]") {
    TokenInterner interner;
    const auto first = interner.Intern("A");
    const auto second = interner.Intern("A");
    REQUIRE(first == second);
}

TEST_CASE("TokenInterner merges whitespace variants even when the non-canonical spelling is interned first",
          "[interner]") {
    TokenInterner interner;
    const auto first = interner.Intern("a    b");
    const auto second = interner.Intern("a b");
    REQUIRE(first == second);
}

TEST_CASE("TokenInterner assigns different ids to different text", "[interner]") {
    TokenInterner interner;
    const auto a = interner.Intern("A");
    const auto b = interner.Intern("B");
    REQUIRE(a != b);
}

TEST_CASE("TokenInterner round-trips text through TextFor", "[interner]") {
    TokenInterner interner;
    const auto token = interner.Intern("hello");
    REQUIRE(interner.TextFor(token) == "hello");
}

TEST_CASE("TokenInterner tracks the number of distinct tokens", "[interner]") {
    TokenInterner interner;
    [[maybe_unused]] const auto a = interner.Intern("A");
    [[maybe_unused]] const auto b = interner.Intern("B");
    [[maybe_unused]] const auto aAgain = interner.Intern("A");
    REQUIRE(interner.Size() == 2);
}

TEST_CASE("TokenInterner::TextFor throws for an unknown token id", "[interner]") {
    TokenInterner interner;
    REQUIRE_THROWS_AS(interner.TextFor(0), std::out_of_range);
}
