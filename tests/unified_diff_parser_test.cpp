#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "git_diff_pivot/diff_document.hpp"
#include "git_diff_pivot/token_interner.hpp"
#include "git_diff_pivot/unified_diff_parser.hpp"

using git_diff_pivot::DiffLineKind;
using git_diff_pivot::TokenInterner;
using git_diff_pivot::UnifiedDiffParser;

TEST_CASE("UnifiedDiffParser parses a single file with one hunk", "[unified_diff_parser]") {
    const std::vector<std::string> lines = {
        "diff --git a/file.txt b/file.txt",
        "index 1111111..2222222 100644",
        "--- a/file.txt",
        "+++ b/file.txt",
        "@@ -1,4 +1,4 @@",
        " context1",
        "-old line",
        "+new line",
        " context2",
    };

    TokenInterner interner;
    const auto document = UnifiedDiffParser::Parse(lines, interner);

    REQUIRE(document.HunkCount() == 1);
    const auto& hunk = document.Hunks().front();
    REQUIRE(hunk.filePath == "file.txt");
    REQUIRE(hunk.lines.size() == 2);
    REQUIRE(hunk.lines[0].kind == DiffLineKind::Removed);
    REQUIRE(hunk.lines[0].lineNumber == 2);
    REQUIRE(hunk.lines[1].kind == DiffLineKind::Added);
    REQUIRE(hunk.lines[1].lineNumber == 2);
}

TEST_CASE("UnifiedDiffParser splits multiple hunks in the same file into separate hunks", "[unified_diff_parser]") {
    const std::vector<std::string> lines = {
        "diff --git a/file.txt b/file.txt",
        "--- a/file.txt",
        "+++ b/file.txt",
        "@@ -1,2 +1,2 @@",
        "-a",
        "+b",
        "@@ -10,2 +10,2 @@",
        "-c",
        "+d",
    };

    TokenInterner interner;
    const auto document = UnifiedDiffParser::Parse(lines, interner);

    REQUIRE(document.HunkCount() == 2);
    REQUIRE(document.Hunks()[0].filePath == "file.txt");
    REQUIRE(document.Hunks()[0].lines.size() == 2);
    REQUIRE(document.Hunks()[0].lines[0].lineNumber == 1);

    REQUIRE(document.Hunks()[1].filePath == "file.txt");
    REQUIRE(document.Hunks()[1].lines.size() == 2);
    REQUIRE(document.Hunks()[1].lines[0].lineNumber == 10);
}

TEST_CASE("UnifiedDiffParser tracks file identity across multiple files", "[unified_diff_parser]") {
    const std::vector<std::string> lines = {
        "diff --git a/a.txt b/a.txt",
        "--- a/a.txt",
        "+++ b/a.txt",
        "@@ -1,1 +1,1 @@",
        "-old a",
        "+new a",
        "diff --git a/b.txt b/b.txt",
        "--- a/b.txt",
        "+++ b/b.txt",
        "@@ -1,1 +1,1 @@",
        "-old b",
        "+new b",
    };

    TokenInterner interner;
    const auto document = UnifiedDiffParser::Parse(lines, interner);

    REQUIRE(document.HunkCount() == 2);
    REQUIRE(document.Hunks()[0].filePath == "a.txt");
    REQUIRE(document.Hunks()[1].filePath == "b.txt");
}

TEST_CASE("UnifiedDiffParser splits hunks on context lines within a hunk", "[unified_diff_parser]") {
    const std::vector<std::string> lines = {
        "diff --git a/file.txt b/file.txt",
        "--- a/file.txt",
        "+++ b/file.txt",
        "@@ -1,6 +1,6 @@",
        "-first old",
        "+first new",
        " middle context",
        "-second old",
        "+second new",
    };

    TokenInterner interner;
    const auto document = UnifiedDiffParser::Parse(lines, interner);

    REQUIRE(document.HunkCount() == 2);
    REQUIRE(document.Hunks()[0].lines.size() == 2);
    REQUIRE(document.Hunks()[1].lines.size() == 2);
}

TEST_CASE("UnifiedDiffParser produces no hunks for a context-only diff", "[unified_diff_parser]") {
    const std::vector<std::string> lines = {
        "diff --git a/file.txt b/file.txt",
        "--- a/file.txt",
        "+++ b/file.txt",
        "@@ -1,2 +1,2 @@",
        " context1",
        " context2",
    };

    TokenInterner interner;
    const auto document = UnifiedDiffParser::Parse(lines, interner);

    REQUIRE(document.HunkCount() == 0);
}

TEST_CASE("UnifiedDiffParser skips binary diff entries without crashing", "[unified_diff_parser]") {
    const std::vector<std::string> lines = {
        "diff --git a/image.png b/image.png",
        "index 1111111..2222222 100644",
        "Binary files a/image.png and b/image.png differ",
        "diff --git a/file.txt b/file.txt",
        "--- a/file.txt",
        "+++ b/file.txt",
        "@@ -1,1 +1,1 @@",
        "-old",
        "+new",
    };

    TokenInterner interner;
    const auto document = UnifiedDiffParser::Parse(lines, interner);

    REQUIRE(document.HunkCount() == 1);
    REQUIRE(document.Hunks()[0].filePath == "file.txt");
}

TEST_CASE("UnifiedDiffParser uses the pre-deletion path for a deleted file", "[unified_diff_parser]") {
    const std::vector<std::string> lines = {
        "diff --git a/gone.txt b/gone.txt",
        "deleted file mode 100644",
        "--- a/gone.txt",
        "+++ /dev/null",
        "@@ -1,1 +0,0 @@",
        "-old",
    };

    TokenInterner interner;
    const auto document = UnifiedDiffParser::Parse(lines, interner);

    REQUIRE(document.HunkCount() == 1);
    REQUIRE(document.Hunks()[0].filePath == "gone.txt");
    REQUIRE(document.Hunks()[0].lines[0].kind == DiffLineKind::Removed);
    REQUIRE(document.Hunks()[0].lines[0].lineNumber == 1);
}

TEST_CASE("UnifiedDiffParser preserves diff operation identity for equal text", "[unified_diff_parser]") {
    const std::vector<std::string> lines = {
        "diff --git a/file.txt b/file.txt",
        "--- a/file.txt",
        "+++ b/file.txt",
        "@@ -1,1 +1,1 @@",
        "-same text",
        "+same text",
    };

    TokenInterner interner;
    const auto document = UnifiedDiffParser::Parse(lines, interner);

    REQUIRE(document.HunkCount() == 1);
    const auto& hunk = document.Hunks().front();
    REQUIRE(hunk.lines.size() == 2);
    REQUIRE(hunk.lines[0].token != hunk.lines[1].token);
}
