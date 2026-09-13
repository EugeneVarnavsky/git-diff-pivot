#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

#include "git_diff_pivot/diff_document.hpp"
#include "git_diff_pivot/token_interner.hpp"

using git_diff_pivot::ChangedLine;
using git_diff_pivot::ChangeHunk;
using git_diff_pivot::DiffDocument;
using git_diff_pivot::DiffLineKind;
using git_diff_pivot::TokenInterner;

namespace {

// Builds one hunk for `filePath` from the given already-normalized letters,
// interning each as a token and numbering lines from 1.
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

}  // namespace

// This fixture captures the canonical example the repeated-change detector
// solves (see repeated_change_detector_test.cpp for detection assertions):
//
//   File A: A B C D
//   File B: A B C E
//   File C: X A B C
//
// "A B C" is recognized as a repeated sequence occurring in all three
// files. This test only checks that the document model can represent the
// scenario faithfully (identical tokens for "A", "B", "C" in every file,
// distinct tokens for "D", "E", "X").
TEST_CASE("DiffDocument can represent the canonical repeated-sequence fixture", "[document][fixture]") {
    TokenInterner interner;

    DiffDocument document;
    document.AddHunk(MakeHunk(interner, "FileA", {"A", "B", "C", "D"}));
    document.AddHunk(MakeHunk(interner, "FileB", {"A", "B", "C", "E"}));
    document.AddHunk(MakeHunk(interner, "FileC", {"X", "A", "B", "C"}));

    REQUIRE(document.HunkCount() == 3);

    const auto& hunkA = document.Hunks()[0];
    const auto& hunkB = document.Hunks()[1];
    const auto& hunkC = document.Hunks()[2];

    // "A B C" must tokenize identically regardless of which file it appears in.
    REQUIRE(hunkA.lines[0].token == hunkB.lines[0].token);
    REQUIRE(hunkA.lines[1].token == hunkB.lines[1].token);
    REQUIRE(hunkA.lines[2].token == hunkB.lines[2].token);

    REQUIRE(hunkA.lines[0].token == hunkC.lines[1].token);
    REQUIRE(hunkA.lines[1].token == hunkC.lines[2].token);
    REQUIRE(hunkA.lines[2].token == hunkC.lines[3].token);

    // "D", "E" and "X" are distinct from "A"/"B"/"C" and from each other.
    const auto tokenD = hunkA.lines[3].token;
    const auto tokenE = hunkB.lines[3].token;
    const auto tokenX = hunkC.lines[0].token;
    REQUIRE(tokenD != tokenE);
    REQUIRE(tokenD != tokenX);
    REQUIRE(tokenE != tokenX);
}
