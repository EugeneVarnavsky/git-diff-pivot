#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "git_diff_pivot/change_selector.hpp"
#include "git_diff_pivot/diff_document.hpp"
#include "git_diff_pivot/output_renderer.hpp"
#include "git_diff_pivot/repeated_change_detector.hpp"
#include "git_diff_pivot/token_interner.hpp"

using git_diff_pivot::ChangedLine;
using git_diff_pivot::ChangeHunk;
using git_diff_pivot::ChangeSelection;
using git_diff_pivot::ChangeSelector;
using git_diff_pivot::DiffDocument;
using git_diff_pivot::DiffLineKind;
using git_diff_pivot::OutputFormat;
using git_diff_pivot::OutputRenderer;
using git_diff_pivot::ParseOutputFormat;
using git_diff_pivot::RepeatedChangeDetector;
using git_diff_pivot::RepeatedSequence;
using git_diff_pivot::RepeatedSequenceOccurrence;
using git_diff_pivot::TokenInterner;
using git_diff_pivot::UniqueLine;

namespace {

ChangeHunk MakeHunk(TokenInterner& interner, std::string filePath,
                     std::initializer_list<std::string_view> letters) {
    ChangeHunk hunk;
    hunk.filePath = std::move(filePath);
    std::uint32_t lineNumber = 1;
    for (const auto letter : letters) {
        hunk.lines.push_back(ChangedLine{
            .token = interner.Intern(letter),
            .lineNumber = lineNumber,
            .kind = DiffLineKind::Added,
        });
        ++lineNumber;
    }
    return hunk;
}

// OutputRenderer::Render writes into a std::ostream; tests compare full strings.
std::string RenderToString(const ChangeSelection& selection, const TokenInterner& interner,
                            OutputFormat format = OutputFormat::Text) {
    std::ostringstream out;
    OutputRenderer(selection, interner).Render(out, format);
    return out.str();
}

}  // namespace

TEST_CASE("OutputRenderer renders an empty selection as zero counts and no sections",
          "[output-renderer]") {
    TokenInterner interner;
    ChangeSelection selection;

    REQUIRE(RenderToString(selection, interner) == "0 common change(s), 0 unique line(s).\n");
}

TEST_CASE("OutputRenderer renders unique lines grouped by file with no common-change section",
          "[output-renderer]") {
    TokenInterner interner;
    const auto tokenA = interner.Intern("A");
    const auto tokenB = interner.Intern("B");

    ChangeSelection selection;
    selection.uniqueLines = {
        UniqueLine{.filePath = "FileA", .line = ChangedLine{.token = tokenA, .lineNumber = 1, .kind = DiffLineKind::Added}},
        UniqueLine{.filePath = "FileB", .line = ChangedLine{.token = tokenB, .lineNumber = 3, .kind = DiffLineKind::Removed}},
    };

    REQUIRE(RenderToString(selection, interner) ==
            "0 common change(s), 2 unique line(s).\n"
            "\n"
            "Unique changes:\n"
            "FileA:\n"
            "  @@ -0,0 +1 @@\n"
            "  A\n"
            "\n"
            "FileB:\n"
            "  @@ -3 +0,0 @@\n"
            "  B\n");
}

TEST_CASE("OutputRenderer renders a common change section with no unique lines", "[output-renderer]") {
    TokenInterner interner;
    const auto tokenA = interner.Intern("A");
    const auto tokenB = interner.Intern("B");

    ChangeSelection selection;
    selection.commonChanges.push_back(RepeatedSequence{
        .tokens = {tokenA, tokenB},
        .occurrences =
            {
                RepeatedSequenceOccurrence{.filePath = "FileA", .startLine = 1, .length = 2},
                RepeatedSequenceOccurrence{.filePath = "FileB", .startLine = 5, .length = 2},
            },
    });

    REQUIRE(RenderToString(selection, interner) ==
            "1 common change(s), 0 unique line(s).\n"
            "\n"
            "Common change 1 (2 occurrence(s), 2 line(s)):\n"
            "  A\n"
            "  B\n"
            "  Occurrences:\n"
            "    FileA:1\n"
            "    FileB:5\n");
}

TEST_CASE("OutputRenderer groups repeated occurrences in the same file under one path in text and Markdown",
          "[output-renderer]") {
    TokenInterner interner;
    const auto tokenA = interner.Intern("A");

    ChangeSelection selection;
    selection.commonChanges.push_back(RepeatedSequence{
        .tokens = {tokenA},
        .occurrences =
            {
                RepeatedSequenceOccurrence{.filePath = "FileA", .startLine = 1, .length = 1},
                RepeatedSequenceOccurrence{.filePath = "FileA", .startLine = 10, .length = 1},
                RepeatedSequenceOccurrence{.filePath = "FileB", .startLine = 5, .length = 1},
                RepeatedSequenceOccurrence{.filePath = "FileA", .startLine = 20, .length = 1},
            },
    });

    REQUIRE(RenderToString(selection, interner) ==
            "1 common change(s), 0 unique line(s).\n"
            "\n"
            "Common change 1 (4 occurrence(s), 1 line(s)):\n"
            "  A\n"
            "  Occurrences:\n"
            "    FileA:1,10,20\n"
            "    FileB:5\n");

    REQUIRE(RenderToString(selection, interner, OutputFormat::Markdown) ==
            "## Compressed diff summary\n"
            "\n"
            "**1 common change(s), 0 unique line(s).**\n"
            "\n"
            "### Common change 1 (4 occurrence(s), 1 line(s))\n"
            "\n"
            "```diff\n"
            "A\n"
            "```\n"
            "\n"
            "**Occurrences:**\n"
            "\n"
            "- `FileA:1,10,20`\n"
            "- `FileB:5`\n");
}

TEST_CASE("OutputRenderer produces the expected compressed output for the canonical fixture",
          "[output-renderer][fixture]") {
    TokenInterner interner;
    DiffDocument document;
    document.AddHunk(MakeHunk(interner, "FileA", {"A", "B", "C", "D"}));
    document.AddHunk(MakeHunk(interner, "FileB", {"A", "B", "C", "E"}));
    document.AddHunk(MakeHunk(interner, "FileC", {"X", "A", "B", "C"}));

    const RepeatedChangeDetector detector;
    const auto candidates = detector.Detect(document);

    const ChangeSelector selector;
    const auto selection = selector.Select(document, candidates);

    REQUIRE(RenderToString(selection, interner) ==
            "1 common change(s), 3 unique line(s).\n"
            "\n"
            "Common change 1 (3 occurrence(s), 3 line(s)):\n"
            "  A\n"
            "  B\n"
            "  C\n"
            "  Occurrences:\n"
            "    FileA:1\n"
            "    FileB:1\n"
            "    FileC:2\n"
            "\n"
            "Unique changes:\n"
            "FileA:\n"
            "  @@ -0,0 +4 @@\n"
            "  D\n"
            "\n"
            "FileB:\n"
            "  @@ -0,0 +4 @@\n"
            "  E\n"
            "\n"
            "FileC:\n"
            "  @@ -0,0 +1 @@\n"
            "  X\n");
}

TEST_CASE("ParseOutputFormat recognizes txt/md/json and rejects unknown values", "[output-renderer]") {
    REQUIRE(ParseOutputFormat("txt") == OutputFormat::Text);
    REQUIRE(ParseOutputFormat("Md") == OutputFormat::Markdown);
    REQUIRE(ParseOutputFormat("JSON") == OutputFormat::Json);
    REQUIRE_FALSE(ParseOutputFormat("xml").has_value());
    REQUIRE_FALSE(ParseOutputFormat("").has_value());
    REQUIRE(ParseOutputFormat("TXT") == OutputFormat::Text);
}

TEST_CASE("OutputRenderer renders Markdown for the canonical fixture", "[output-renderer][fixture]") {
    TokenInterner interner;
    DiffDocument document;
    document.AddHunk(MakeHunk(interner, "FileA", {"A", "B", "C", "D"}));
    document.AddHunk(MakeHunk(interner, "FileB", {"A", "B", "C", "E"}));
    document.AddHunk(MakeHunk(interner, "FileC", {"X", "A", "B", "C"}));

    const RepeatedChangeDetector detector;
    const auto candidates = detector.Detect(document);

    const ChangeSelector selector;
    const auto selection = selector.Select(document, candidates);

    REQUIRE(RenderToString(selection, interner, OutputFormat::Markdown) ==
            "## Compressed diff summary\n"
            "\n"
            "**1 common change(s), 3 unique line(s).**\n"
            "\n"
            "### Common change 1 (3 occurrence(s), 3 line(s))\n"
            "\n"
            "```diff\n"
            "A\n"
            "B\n"
            "C\n"
            "```\n"
            "\n"
            "**Occurrences:**\n"
            "\n"
            "- `FileA:1`\n"
            "- `FileB:1`\n"
            "- `FileC:2`\n"
            "\n"
            "### Unique changes\n"
            "\n"
            "**FileA**\n"
            "\n"
            "```diff\n"
            "@@ -0,0 +4 @@\n"
            "D\n"
            "```\n"
            "\n"
            "**FileB**\n"
            "\n"
            "```diff\n"
            "@@ -0,0 +4 @@\n"
            "E\n"
            "```\n"
            "\n"
            "**FileC**\n"
            "\n"
            "```diff\n"
            "@@ -0,0 +1 @@\n"
            "X\n"
            "```\n");
}

TEST_CASE("OutputRenderer renders JSON for the canonical fixture", "[output-renderer][fixture]") {
    TokenInterner interner;
    DiffDocument document;
    document.AddHunk(MakeHunk(interner, "FileA", {"A", "B", "C", "D"}));
    document.AddHunk(MakeHunk(interner, "FileB", {"A", "B", "C", "E"}));
    document.AddHunk(MakeHunk(interner, "FileC", {"X", "A", "B", "C"}));

    const RepeatedChangeDetector detector;
    const auto candidates = detector.Detect(document);

    const ChangeSelector selector;
    const auto selection = selector.Select(document, candidates);

    REQUIRE(RenderToString(selection, interner, OutputFormat::Json) ==
            "{\n"
            "  \"commonChanges\": [\n"
            "    {\n"
            "      \"length\": 3,\n"
            "      \"occurrenceCount\": 3,\n"
            "      \"lines\": [\n"
            "        \"A\",\n"
            "        \"B\",\n"
            "        \"C\"\n"
            "      ],\n"
            "      \"occurrences\": [\n"
            "        {\n"
            "          \"filePath\": \"FileA\",\n"
            "          \"startLine\": 1\n"
            "        },\n"
            "        {\n"
            "          \"filePath\": \"FileB\",\n"
            "          \"startLine\": 1\n"
            "        },\n"
            "        {\n"
            "          \"filePath\": \"FileC\",\n"
            "          \"startLine\": 2\n"
            "        }\n"
            "      ]\n"
            "    }\n"
            "  ],\n"
            "  \"uniqueChanges\": [\n"
            "    {\n"
            "      \"filePath\": \"FileA\",\n"
            "      \"hunks\": [\n"
            "        {\n"
            "          \"oldStart\": 0,\n"
            "          \"oldCount\": 0,\n"
            "          \"newStart\": 4,\n"
            "          \"newCount\": 1,\n"
            "          \"lines\": [\n"
            "            \"D\"\n"
            "          ]\n"
            "        }\n"
            "      ]\n"
            "    },\n"
            "    {\n"
            "      \"filePath\": \"FileB\",\n"
            "      \"hunks\": [\n"
            "        {\n"
            "          \"oldStart\": 0,\n"
            "          \"oldCount\": 0,\n"
            "          \"newStart\": 4,\n"
            "          \"newCount\": 1,\n"
            "          \"lines\": [\n"
            "            \"E\"\n"
            "          ]\n"
            "        }\n"
            "      ]\n"
            "    },\n"
            "    {\n"
            "      \"filePath\": \"FileC\",\n"
            "      \"hunks\": [\n"
            "        {\n"
            "          \"oldStart\": 0,\n"
            "          \"oldCount\": 0,\n"
            "          \"newStart\": 1,\n"
            "          \"newCount\": 1,\n"
            "          \"lines\": [\n"
            "            \"X\"\n"
            "          ]\n"
            "        }\n"
            "      ]\n"
            "    }\n"
            "  ]\n"
            "}\n");
}

TEST_CASE("OutputRenderer renders empty JSON arrays for an empty selection", "[output-renderer]") {
    TokenInterner interner;
    ChangeSelection selection;

    REQUIRE(RenderToString(selection, interner, OutputFormat::Json) ==
            "{\n"
            "  \"commonChanges\": [],\n"
            "  \"uniqueChanges\": []\n"
            "}\n");
}

TEST_CASE("OutputRenderer omits a common change and unique lines that are entirely blank",
          "[output-renderer]") {
    TokenInterner interner;
    const auto blankAdded = interner.Intern("+");
    const auto blankRemoved = interner.Intern("-");
    const auto tokenA = interner.Intern("A");

    ChangeSelection selection;
    selection.commonChanges.push_back(RepeatedSequence{
        .tokens = {blankAdded, blankAdded},
        .occurrences =
            {
                RepeatedSequenceOccurrence{.filePath = "FileA", .startLine = 1, .length = 2},
                RepeatedSequenceOccurrence{.filePath = "FileB", .startLine = 1, .length = 2},
            },
    });
    selection.uniqueLines = {
        UniqueLine{.filePath = "FileA",
                      .line = ChangedLine{.token = blankRemoved, .lineNumber = 5, .kind = DiffLineKind::Removed}},
        UniqueLine{.filePath = "FileA",
                      .line = ChangedLine{.token = tokenA, .lineNumber = 6, .kind = DiffLineKind::Added}},
    };

    REQUIRE(RenderToString(selection, interner) ==
            "0 common change(s), 1 unique line(s).\n"
            "\n"
            "Unique changes:\n"
            "FileA:\n"
            "  @@ -0,0 +6 @@\n"
            "  A\n");
}

TEST_CASE("OutputRenderer keeps a common change that mixes blank and non-blank lines",
          "[output-renderer]") {
    TokenInterner interner;
    const auto blankAdded = interner.Intern("+");
    const auto contentAdded = interner.Intern("+content");

    ChangeSelection selection;
    selection.commonChanges.push_back(RepeatedSequence{
        .tokens = {blankAdded, contentAdded},
        .occurrences =
            {
                RepeatedSequenceOccurrence{.filePath = "FileA", .startLine = 1, .length = 2},
                RepeatedSequenceOccurrence{.filePath = "FileB", .startLine = 1, .length = 2},
            },
    });

    REQUIRE(RenderToString(selection, interner) ==
            "1 common change(s), 0 unique line(s).\n"
            "\n"
            "Common change 1 (2 occurrence(s), 2 line(s)):\n"
            "  +\n"
            "  +content\n"
            "  Occurrences:\n"
            "    FileA:1\n"
            "    FileB:1\n");
}

TEST_CASE("OutputRenderer omits blank-only changes from Markdown and JSON output",
          "[output-renderer]") {
    TokenInterner interner;
    const auto blankAdded = interner.Intern("+");
    const auto tokenA = interner.Intern("A");

    ChangeSelection selection;
    selection.commonChanges.push_back(RepeatedSequence{
        .tokens = {blankAdded},
        .occurrences =
            {
                RepeatedSequenceOccurrence{.filePath = "FileA", .startLine = 1, .length = 1},
                RepeatedSequenceOccurrence{.filePath = "FileB", .startLine = 1, .length = 1},
            },
    });
    selection.uniqueLines = {
        UniqueLine{.filePath = "FileA",
                      .line = ChangedLine{.token = tokenA, .lineNumber = 1, .kind = DiffLineKind::Added}},
    };

    REQUIRE(RenderToString(selection, interner, OutputFormat::Markdown) ==
            "## Compressed diff summary\n"
            "\n"
            "**0 common change(s), 1 unique line(s).**\n"
            "\n"
            "### Unique changes\n"
            "\n"
            "**FileA**\n"
            "\n"
            "```diff\n"
            "@@ -0,0 +1 @@\n"
            "A\n"
            "```\n");
    REQUIRE(RenderToString(selection, interner, OutputFormat::Json) ==
            "{\n"
            "  \"commonChanges\": [],\n"
            "  \"uniqueChanges\": [\n"
            "    {\n"
            "      \"filePath\": \"FileA\",\n"
            "      \"hunks\": [\n"
            "        {\n"
            "          \"oldStart\": 0,\n"
            "          \"oldCount\": 0,\n"
            "          \"newStart\": 1,\n"
            "          \"newCount\": 1,\n"
            "          \"lines\": [\n"
            "            \"A\"\n"
            "          ]\n"
            "        }\n"
            "      ]\n"
            "    }\n"
            "  ]\n"
            "}\n");
}

TEST_CASE("OutputRenderer separates unique lines from different original hunks with a git-style hunk separator",
          "[output-renderer]") {
    TokenInterner interner;
    const auto tokenA = interner.Intern("A");
    const auto tokenB = interner.Intern("B");
    const auto tokenC = interner.Intern("C");

    ChangeSelection selection;
    selection.uniqueLines = {
        // FileA: two lines from hunk 0, then one line from a later hunk 1.
        UniqueLine{.filePath = "FileA",
                   .line = ChangedLine{.token = tokenA, .lineNumber = 1, .kind = DiffLineKind::Added},
                   .hunkIndex = 0},
        UniqueLine{.filePath = "FileA",
                   .line = ChangedLine{.token = tokenB, .lineNumber = 2, .kind = DiffLineKind::Added},
                   .hunkIndex = 0},
        UniqueLine{.filePath = "FileA",
                   .line = ChangedLine{.token = tokenC, .lineNumber = 10, .kind = DiffLineKind::Added},
                   .hunkIndex = 1},
    };

    REQUIRE(RenderToString(selection, interner) ==
            "0 common change(s), 3 unique line(s).\n"
            "\n"
            "Unique changes:\n"
            "FileA:\n"
            "  @@ -0,0 +1,2 @@\n"
            "  A\n"
            "  B\n"
            "\n"
            "  @@ -0,0 +10 @@\n"
            "  C\n");
    REQUIRE(RenderToString(selection, interner, OutputFormat::Markdown) ==
            "## Compressed diff summary\n"
            "\n"
            "**0 common change(s), 3 unique line(s).**\n"
            "\n"
            "### Unique changes\n"
            "\n"
            "**FileA**\n"
            "\n"
            "```diff\n"
            "@@ -0,0 +1,2 @@\n"
            "A\n"
            "B\n"
            "@@ -0,0 +10 @@\n"
            "C\n"
            "```\n");
    REQUIRE(RenderToString(selection, interner, OutputFormat::Json) ==
            "{\n"
            "  \"commonChanges\": [],\n"
            "  \"uniqueChanges\": [\n"
            "    {\n"
            "      \"filePath\": \"FileA\",\n"
            "      \"hunks\": [\n"
            "        {\n"
            "          \"oldStart\": 0,\n"
            "          \"oldCount\": 0,\n"
            "          \"newStart\": 1,\n"
            "          \"newCount\": 2,\n"
            "          \"lines\": [\n"
            "            \"A\",\n"
            "            \"B\"\n"
            "          ]\n"
            "        },\n"
            "        {\n"
            "          \"oldStart\": 0,\n"
            "          \"oldCount\": 0,\n"
            "          \"newStart\": 10,\n"
            "          \"newCount\": 1,\n"
            "          \"lines\": [\n"
            "            \"C\"\n"
            "          ]\n"
            "        }\n"
            "      ]\n"
            "    }\n"
            "  ]\n"
            "}\n");
}

TEST_CASE("OutputRenderer escapes quotes, backslashes, and control characters in JSON", "[output-renderer]") {
    TokenInterner interner;
    const auto token = interner.Intern("say \"hi\"\\then\ttab");

    ChangeSelection selection;
    selection.uniqueLines = {
        UniqueLine{.filePath = "File\"A\"",
                      .line = ChangedLine{.token = token, .lineNumber = 1, .kind = DiffLineKind::Added}},
    };

    REQUIRE(RenderToString(selection, interner, OutputFormat::Json) ==
            "{\n"
            "  \"commonChanges\": [],\n"
            "  \"uniqueChanges\": [\n"
            "    {\n"
            "      \"filePath\": \"File\\\"A\\\"\",\n"
            "      \"hunks\": [\n"
            "        {\n"
            "          \"oldStart\": 0,\n"
            "          \"oldCount\": 0,\n"
            "          \"newStart\": 1,\n"
            "          \"newCount\": 1,\n"
            "          \"lines\": [\n"
            "            \"say \\\"hi\\\"\\\\then\\ttab\"\n"
            "          ]\n"
            "        }\n"
            "      ]\n"
            "    }\n"
            "  ]\n"
            "}\n");
}
