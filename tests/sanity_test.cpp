#include <catch2/catch_test_macros.hpp>

TEST_CASE("project builds and the test runner executes", "[sanity]") {
    REQUIRE(1 + 1 == 2);
}
