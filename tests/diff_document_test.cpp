#include <catch2/catch_test_macros.hpp>

#include "git_diff_pivot/diff_document.hpp"

using git_diff_pivot::ChangedLine;
using git_diff_pivot::ChangeHunk;
using git_diff_pivot::DiffDocument;
using git_diff_pivot::DiffLineKind;

TEST_CASE("DiffDocument starts empty", "[document]") {
    DiffDocument document;
    REQUIRE(document.HunkCount() == 0);
    REQUIRE(document.Hunks().empty());
}

TEST_CASE("DiffDocument preserves file identity and line positions", "[document]") {
    DiffDocument document;

    ChangeHunk hunk;
    hunk.filePath = "src/example.cpp";
    hunk.lines.push_back(ChangedLine{.token = 0, .lineNumber = 10, .kind = DiffLineKind::Added});
    hunk.lines.push_back(ChangedLine{.token = 1, .lineNumber = 11, .kind = DiffLineKind::Removed});

    document.AddHunk(hunk);

    REQUIRE(document.HunkCount() == 1);
    const auto& storedHunk = document.Hunks().front();
    REQUIRE(storedHunk.filePath == "src/example.cpp");
    REQUIRE(storedHunk.lines.size() == 2);
    REQUIRE(storedHunk.lines[0].lineNumber == 10);
    REQUIRE(storedHunk.lines[1].lineNumber == 11);
    REQUIRE(storedHunk.lines[0].kind == DiffLineKind::Added);
    REQUIRE(storedHunk.lines[1].kind == DiffLineKind::Removed);
}

TEST_CASE("DiffDocument keeps hunks from different files separate", "[document]") {
    DiffDocument document;

    ChangeHunk first;
    first.filePath = "a.txt";
    first.lines.push_back(ChangedLine{.token = 0, .lineNumber = 1, .kind = DiffLineKind::Added});

    ChangeHunk second;
    second.filePath = "b.txt";
    second.lines.push_back(ChangedLine{.token = 0, .lineNumber = 1, .kind = DiffLineKind::Added});

    document.AddHunk(first);
    document.AddHunk(second);

    REQUIRE(document.HunkCount() == 2);
    REQUIRE(document.Hunks()[0].filePath == "a.txt");
    REQUIRE(document.Hunks()[1].filePath == "b.txt");
}
