#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>
#include <vector>

#include "cli/cli_options.hpp"
#include "git_diff_pivot/output_renderer.hpp"

using git_diff_pivot::CliOptions;
using git_diff_pivot::kDefaultMinOccurrenceCount;
using git_diff_pivot::kDefaultMinSequenceLength;
using git_diff_pivot::OutputFormat;
using git_diff_pivot::ParseCliOptions;

TEST_CASE("ParseCliOptions returns documented defaults for no arguments", "[cli-options]") {
    const CliOptions options = ParseCliOptions({});

    REQUIRE_FALSE(options.helpRequested);
    REQUIRE_FALSE(options.versionRequested);
    REQUIRE_FALSE(options.inputPath.has_value());
    REQUIRE_FALSE(options.outputPath.has_value());
    REQUIRE(options.minSequenceLength == kDefaultMinSequenceLength);
    REQUIRE(options.minOccurrenceCount == kDefaultMinOccurrenceCount);
    REQUIRE(options.outputFormat == OutputFormat::Text);
}

TEST_CASE("ParseCliOptions recognizes -h and --help", "[cli-options]") {
    REQUIRE(ParseCliOptions({"-h"}).helpRequested);
    REQUIRE(ParseCliOptions({"--help"}).helpRequested);
}

TEST_CASE("ParseCliOptions recognizes -v and --version", "[cli-options]") {
    REQUIRE(ParseCliOptions({"-v"}).versionRequested);
    REQUIRE(ParseCliOptions({"--version"}).versionRequested);
}

TEST_CASE("ParseCliOptions accepts a positional input path", "[cli-options]") {
    const CliOptions options = ParseCliOptions({"diff.patch"});
    REQUIRE(options.inputPath == "diff.patch");
}

TEST_CASE("ParseCliOptions parses --min-length and --min-occurrences via a separate argument", "[cli-options]") {
    const CliOptions options = ParseCliOptions({"--min-length", "5", "--min-occurrences", "3"});
    REQUIRE(options.minSequenceLength == 5);
    REQUIRE(options.minOccurrenceCount == 3);
}

TEST_CASE("ParseCliOptions parses --min-length and --min-occurrences via '='", "[cli-options]") {
    const CliOptions options = ParseCliOptions({"--min-length=5", "--min-occurrences=3"});
    REQUIRE(options.minSequenceLength == 5);
    REQUIRE(options.minOccurrenceCount == 3);
}

TEST_CASE("ParseCliOptions rejects a missing threshold value", "[cli-options]") {
    REQUIRE_THROWS_AS(ParseCliOptions({"--min-length"}), std::invalid_argument);
}

TEST_CASE("ParseCliOptions rejects a non-numeric or zero threshold value", "[cli-options]") {
    REQUIRE_THROWS_AS(ParseCliOptions({"--min-length", "abc"}), std::invalid_argument);
    REQUIRE_THROWS_AS(ParseCliOptions({"--min-length", "0"}), std::invalid_argument);
}

TEST_CASE("ParseCliOptions parses --output via a separate argument and via '='", "[cli-options]") {
    REQUIRE(ParseCliOptions({"--output", "out.txt"}).outputPath == "out.txt");
    REQUIRE(ParseCliOptions({"--output=out.txt"}).outputPath == "out.txt");
}

TEST_CASE("ParseCliOptions parses --output-type via a separate argument and via '='", "[cli-options]") {
    REQUIRE(ParseCliOptions({"--output-type", "json"}).outputFormat == OutputFormat::Json);
    REQUIRE(ParseCliOptions({"--output-type=json"}).outputFormat == OutputFormat::Json);
}

TEST_CASE("ParseCliOptions rejects an invalid --output-type value", "[cli-options]") {
    REQUIRE_THROWS_AS(ParseCliOptions({"--output-type", "xml"}), std::invalid_argument);
}

TEST_CASE("ParseCliOptions infers --output-type from the --output file extension", "[cli-options]") {
    REQUIRE(ParseCliOptions({"--output", "out.txt"}).outputFormat == OutputFormat::Text);
    REQUIRE(ParseCliOptions({"--output", "out.md"}).outputFormat == OutputFormat::Markdown);
    REQUIRE(ParseCliOptions({"--output", "out.json"}).outputFormat == OutputFormat::Json);
}

TEST_CASE("ParseCliOptions rejects an --output path whose extension is not a known format", "[cli-options]") {
    REQUIRE_THROWS_AS(ParseCliOptions({"--output", "out.xml"}), std::invalid_argument);
}

TEST_CASE("ParseCliOptions lets an explicit --output-type override the inferred extension", "[cli-options]") {
    const CliOptions options = ParseCliOptions({"--output", "out.txt", "--output-type", "json"});
    REQUIRE(options.outputFormat == OutputFormat::Json);
}

TEST_CASE("ParseCliOptions honors --output-type without --output, affecting only the format", "[cli-options]") {
    const CliOptions options = ParseCliOptions({"--output-type", "md"});
    REQUIRE_FALSE(options.outputPath.has_value());
    REQUIRE(options.outputFormat == OutputFormat::Markdown);
}
