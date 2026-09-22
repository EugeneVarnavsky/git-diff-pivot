#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "git_diff_pivot/change_selector.hpp"
#include "git_diff_pivot/diff_document.hpp"
#include "git_diff_pivot/output_renderer.hpp"
#include "git_diff_pivot/repeated_change_detector.hpp"
#include "git_diff_pivot/token_interner.hpp"
#include "git_diff_pivot/unified_diff_parser.hpp"

using git_diff_pivot::ChangeSelection;
using git_diff_pivot::ChangeSelector;
using git_diff_pivot::DiffDocument;
using git_diff_pivot::OutputFormat;
using git_diff_pivot::OutputRenderer;
using git_diff_pivot::RepeatedChangeDetector;
using git_diff_pivot::TokenInterner;
using git_diff_pivot::UnifiedDiffParser;

namespace {

// OutputRenderer::Render writes into a std::ostream; tests compare full strings.
std::string RenderToString(const ChangeSelection& selection, const TokenInterner& interner,
                            OutputFormat format = OutputFormat::Text) {
    std::ostringstream out;
    OutputRenderer(selection, interner).Render(out, format);
    return out.str();
}

}  // namespace

TEST_CASE("Pipeline integration: complex deterministic diff produces expected compressed output",
          "[integration]") {
    // Three files each add the same three-line sequence (a repeated common
    // change), plus one file-specific line each, so the expected output has
    // one common change (3 occurrences) and three unique lines.
    const std::string diffText =
        "diff --git a/src/alpha.cpp b/src/alpha.cpp\n"
        "--- a/src/alpha.cpp\n"
        "+++ b/src/alpha.cpp\n"
        "@@ -0,0 +1,4 @@\n"
        "+int result = 0;\n"
        "+result = compute();\n"
        "+return result;\n"
        "+// alpha-specific line\n"
        "diff --git a/src/beta.cpp b/src/beta.cpp\n"
        "--- a/src/beta.cpp\n"
        "+++ b/src/beta.cpp\n"
        "@@ -0,0 +1,4 @@\n"
        "+int result = 0;\n"
        "+result = compute();\n"
        "+return result;\n"
        "+// beta-specific line\n"
        "diff --git a/src/gamma.cpp b/src/gamma.cpp\n"
        "--- a/src/gamma.cpp\n"
        "+++ b/src/gamma.cpp\n"
        "@@ -0,0 +1,4 @@\n"
        "+int result = 0;\n"
        "+result = compute();\n"
        "+return result;\n"
        "+// gamma-specific line\n";

    TokenInterner interner;
    std::istringstream diffStream(diffText);
    const DiffDocument document = UnifiedDiffParser::Parse(diffStream, interner);

    REQUIRE(document.HunkCount() == 3);

    const RepeatedChangeDetector detector(/*minSequenceLength=*/1);
    const auto candidates = detector.Detect(document);

    const ChangeSelector selector(/*minSequenceLength=*/1, /*minOccurrenceCount=*/2);
    const ChangeSelection selection = selector.Select(document, candidates);

    // Exactly one accepted common change: the shared 3-line sequence.
    REQUIRE(selection.commonChanges.size() == 1);
    const auto& common = selection.commonChanges.front();
    CHECK(common.tokens.size() == 3);
    CHECK(common.occurrences.size() == 3);

    // Exactly three unique lines, one per file's file-specific comment.
    REQUIRE(selection.uniqueLines.size() == 3);

    const std::string rendered = RenderToString(selection, interner);

    // The rendered output must mention the shared sequence once, list all
    // three occurrences, and list all three file-specific lines under their
    // own files.
    CHECK(rendered.find("Common change") != std::string::npos);
    CHECK(rendered.find("src/alpha.cpp") != std::string::npos);
    CHECK(rendered.find("src/beta.cpp") != std::string::npos);
    CHECK(rendered.find("src/gamma.cpp") != std::string::npos);
    CHECK(rendered.find("+int result = 0;") != std::string::npos);
    CHECK(rendered.find("+// alpha-specific line") != std::string::npos);
    CHECK(rendered.find("+// beta-specific line") != std::string::npos);
    CHECK(rendered.find("+// gamma-specific line") != std::string::npos);

    // Deterministic: rendering the same selection twice yields identical text.
    const std::string renderedAgain = RenderToString(selection, interner);
    CHECK(rendered == renderedAgain);
}

TEST_CASE("Pipeline integration: diff with no repeats yields only unique lines",
          "[integration]") {
    const std::string diffText =
        "diff --git a/src/one.cpp b/src/one.cpp\n"
        "--- a/src/one.cpp\n"
        "+++ b/src/one.cpp\n"
        "@@ -0,0 +1,2 @@\n"
        "+unique line one\n"
        "+unique line two\n";

    TokenInterner interner;
    std::istringstream diffStream(diffText);
    const DiffDocument document = UnifiedDiffParser::Parse(diffStream, interner);

    const RepeatedChangeDetector detector(/*minSequenceLength=*/1);
    const auto candidates = detector.Detect(document);

    const ChangeSelector selector(/*minSequenceLength=*/1, /*minOccurrenceCount=*/2);
    const ChangeSelection selection = selector.Select(document, candidates);

    CHECK(selection.commonChanges.empty());
    REQUIRE(selection.uniqueLines.size() == 2);

    const std::string rendered = RenderToString(selection, interner);

    CHECK(rendered.find("Common change") == std::string::npos);
    CHECK(rendered.find("+unique line one") != std::string::npos);
    CHECK(rendered.find("+unique line two") != std::string::npos);
}

TEST_CASE("Pipeline integration: overlapping candidates resolve to the longest common change",
          "[integration]") {
    // Each file repeats "shared one"/"shared two"/"shared three", so the
    // detector also finds shorter overlapping candidates (e.g. the last two
    // lines alone); selection must keep only the full 3-line change.
    const std::string diffText =
        "diff --git a/src/one.cpp b/src/one.cpp\n"
        "--- a/src/one.cpp\n"
        "+++ b/src/one.cpp\n"
        "@@ -0,0 +1,3 @@\n"
        "+shared one\n"
        "+shared two\n"
        "+shared three\n"
        "diff --git a/src/two.cpp b/src/two.cpp\n"
        "--- a/src/two.cpp\n"
        "+++ b/src/two.cpp\n"
        "@@ -0,0 +1,3 @@\n"
        "+shared one\n"
        "+shared two\n"
        "+shared three\n";

    TokenInterner interner;
    std::istringstream diffStream(diffText);
    const DiffDocument document = UnifiedDiffParser::Parse(diffStream, interner);

    const RepeatedChangeDetector detector(/*minSequenceLength=*/1);
    const auto candidates = detector.Detect(document);
    REQUIRE(candidates.size() > 1);  // The detector finds overlapping sub-sequences too.

    const ChangeSelector selector(/*minSequenceLength=*/1, /*minOccurrenceCount=*/2);
    const ChangeSelection selection = selector.Select(document, candidates);

    // Selection must resolve the overlap down to exactly one accepted change.
    REQUIRE(selection.commonChanges.size() == 1);
    CHECK(selection.commonChanges.front().tokens.size() == 3);
    CHECK(selection.uniqueLines.empty());
}

TEST_CASE("Pipeline integration: a common change consisting only of blank lines is omitted from output",
          "[integration]") {
    // Each file adds a single blank line, bounded on both sides by context
    // lines, so the hunk (and the repeated sequence found within it) is
    // exactly one blank added line.
    const std::string diffText =
        "diff --git a/src/alpha.cpp b/src/alpha.cpp\n"
        "--- a/src/alpha.cpp\n"
        "+++ b/src/alpha.cpp\n"
        "@@ -1,2 +1,3 @@\n"
        " #include <a>\n"
        "+\n"
        " #include <b>\n"
        "diff --git a/src/beta.cpp b/src/beta.cpp\n"
        "--- a/src/beta.cpp\n"
        "+++ b/src/beta.cpp\n"
        "@@ -1,2 +1,3 @@\n"
        " #include <a>\n"
        "+\n"
        " #include <b>\n"
        "diff --git a/src/gamma.cpp b/src/gamma.cpp\n"
        "--- a/src/gamma.cpp\n"
        "+++ b/src/gamma.cpp\n"
        "@@ -1,2 +1,3 @@\n"
        " #include <a>\n"
        "+\n"
        " #include <b>\n";

    TokenInterner interner;
    std::istringstream diffStream(diffText);
    const DiffDocument document = UnifiedDiffParser::Parse(diffStream, interner);

    const RepeatedChangeDetector detector(/*minSequenceLength=*/1);
    const auto candidates = detector.Detect(document);

    const ChangeSelector selector(/*minSequenceLength=*/1, /*minOccurrenceCount=*/2);
    const ChangeSelection selection = selector.Select(document, candidates);

    // Detection/selection still see the blank-line repeat; only rendering
    // must hide it.
    REQUIRE(selection.commonChanges.size() == 1);
    CHECK(selection.uniqueLines.empty());

    CHECK(RenderToString(selection, interner) == "0 common change(s), 0 unique line(s).\n");
    CHECK(RenderToString(selection, interner, git_diff_pivot::OutputFormat::Markdown) ==
          "**0 common change(s), 0 unique line(s).**\n");
    CHECK(RenderToString(selection, interner, git_diff_pivot::OutputFormat::Json) ==
          "{\n"
          "  \"commonChanges\": [],\n"
          "  \"uniqueChanges\": []\n"
          "}\n");
}

TEST_CASE(
    "Pipeline integration: repeated changes differing only in whitespace are treated as one common change",
    "[integration]") {
    // Same three-line change added to three files, but each occurrence uses a
    // different amount of insignificant whitespace (single spaces, doubled
    // spaces, tabs). Normalization must make all three collapse onto the same
    // tokens, so they are detected/selected/rendered as a single common
    // change rather than three unrelated unique lines.
    const std::string diffText =
        "diff --git a/src/alpha.cpp b/src/alpha.cpp\n"
        "--- a/src/alpha.cpp\n"
        "+++ b/src/alpha.cpp\n"
        "@@ -0,0 +1,4 @@\n"
        "+int result = 0;\n"
        "+result = compute();\n"
        "+return result;\n"
        "+// alpha-specific line\n"
        "diff --git a/src/beta.cpp b/src/beta.cpp\n"
        "--- a/src/beta.cpp\n"
        "+++ b/src/beta.cpp\n"
        "@@ -0,0 +1,4 @@\n"
        "+int  result  =  0;\n"
        "+result  =  compute();\n"
        "+return  result;\n"
        "+// beta-specific line\n"
        "diff --git a/src/gamma.cpp b/src/gamma.cpp\n"
        "--- a/src/gamma.cpp\n"
        "+++ b/src/gamma.cpp\n"
        "@@ -0,0 +1,4 @@\n"
        "+int\tresult\t=\t0;\n"
        "+result\t=\tcompute();\n"
        "+return\tresult;\n"
        "+// gamma-specific line\n";

    TokenInterner interner;
    std::istringstream diffStream(diffText);
    const DiffDocument document = UnifiedDiffParser::Parse(diffStream, interner);

    REQUIRE(document.HunkCount() == 3);

    const RepeatedChangeDetector detector(/*minSequenceLength=*/1);
    const auto candidates = detector.Detect(document);

    const ChangeSelector selector(/*minSequenceLength=*/1, /*minOccurrenceCount=*/2);
    const ChangeSelection selection = selector.Select(document, candidates);

    // The whitespace-only differences must not prevent the three occurrences
    // from being recognized as the same repeated change.
    REQUIRE(selection.commonChanges.size() == 1);
    const auto& common = selection.commonChanges.front();
    CHECK(common.tokens.size() == 3);
    REQUIRE(common.occurrences.size() == 3);

    // Each file's differently-spaced occurrence is still recorded.
    std::vector<std::string> occurrenceFiles;
    for (const auto& occurrence : common.occurrences) {
        occurrenceFiles.push_back(occurrence.filePath);
    }
    CHECK(std::find(occurrenceFiles.begin(), occurrenceFiles.end(), "src/alpha.cpp") != occurrenceFiles.end());
    CHECK(std::find(occurrenceFiles.begin(), occurrenceFiles.end(), "src/beta.cpp") != occurrenceFiles.end());
    CHECK(std::find(occurrenceFiles.begin(), occurrenceFiles.end(), "src/gamma.cpp") != occurrenceFiles.end());

    // File-specific comments still differ in content, so they stay unique.
    REQUIRE(selection.uniqueLines.size() == 3);

    const std::string rendered = RenderToString(selection, interner);

    // Rendered exactly once as a single common change, using one canonical
    // spelling; the differently-spaced variants must not appear separately.
    CHECK(rendered.find("Common change") != std::string::npos);
    CHECK(rendered.find("+int result = 0;") != std::string::npos);
    CHECK(rendered.find("+int  result  =  0;") == std::string::npos);
    CHECK(rendered.find("+int\tresult\t=\t0;") == std::string::npos);

    // Still lists all three files as occurrences of the one common change.
    CHECK(rendered.find("src/alpha.cpp") != std::string::npos);
    CHECK(rendered.find("src/beta.cpp") != std::string::npos);
    CHECK(rendered.find("src/gamma.cpp") != std::string::npos);
}

TEST_CASE("Pipeline integration: a unique blank-line change is omitted from output",
          "[integration]") {
    const std::string diffText =
        "diff --git a/src/one.cpp b/src/one.cpp\n"
        "--- a/src/one.cpp\n"
        "+++ b/src/one.cpp\n"
        "@@ -1,1 +1,2 @@\n"
        " #include <a>\n"
        "+\n";

    TokenInterner interner;
    std::istringstream diffStream(diffText);
    const DiffDocument document = UnifiedDiffParser::Parse(diffStream, interner);

    const RepeatedChangeDetector detector(/*minSequenceLength=*/1);
    const auto candidates = detector.Detect(document);

    const ChangeSelector selector(/*minSequenceLength=*/1, /*minOccurrenceCount=*/2);
    const ChangeSelection selection = selector.Select(document, candidates);

    REQUIRE(selection.uniqueLines.size() == 1);

    CHECK(RenderToString(selection, interner) == "0 common change(s), 0 unique line(s).\n");
}

TEST_CASE("Pipeline integration: unique-change hunk header uses actual old/new line numbers for a replacement",
          "[integration]") {
    const std::string diffText =
        "diff --git a/src/one.cpp b/src/one.cpp\n"
        "--- a/src/one.cpp\n"
        "+++ b/src/one.cpp\n"
        "@@ -5,3 +5,4 @@\n"
        " context line\n"
        "-old line\n"
        "+new line\n"
        "+added extra line\n"
        " context line2\n";

    TokenInterner interner;
    std::istringstream diffStream(diffText);
    const DiffDocument document = UnifiedDiffParser::Parse(diffStream, interner);

    const ChangeSelector selector;
    const ChangeSelection selection = selector.Select(document, {});
    REQUIRE(selection.uniqueLines.size() == 3);

    CHECK(RenderToString(selection, interner) ==
          "0 common change(s), 3 unique line(s).\n"
          "\n"
          "Unique changes:\n"
          "\n"
          "src/one.cpp:\n"
          "  @@ -6 +6,2 @@\n"
          "  -old line\n"
          "  +new line\n"
          "  +added extra line\n");
}

TEST_CASE("Pipeline integration: unique-change hunk header falls back to the hunk anchor for a pure insertion",
          "[integration]") {
    const std::string diffText =
        "diff --git a/src/two.cpp b/src/two.cpp\n"
        "--- a/src/two.cpp\n"
        "+++ b/src/two.cpp\n"
        "@@ -10,2 +10,4 @@\n"
        " ctx\n"
        "+added1\n"
        "+added2\n"
        " ctx2\n";

    TokenInterner interner;
    std::istringstream diffStream(diffText);
    const DiffDocument document = UnifiedDiffParser::Parse(diffStream, interner);

    const ChangeSelector selector;
    const ChangeSelection selection = selector.Select(document, {});
    REQUIRE(selection.uniqueLines.size() == 2);

    CHECK(RenderToString(selection, interner) ==
          "0 common change(s), 2 unique line(s).\n"
          "\n"
          "Unique changes:\n"
          "\n"
          "src/two.cpp:\n"
          "  @@ -11,0 +11,2 @@\n"
          "  +added1\n"
          "  +added2\n");
}

