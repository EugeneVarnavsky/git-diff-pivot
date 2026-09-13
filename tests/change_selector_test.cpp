#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "git_diff_pivot/change_selector.hpp"
#include "git_diff_pivot/diff_document.hpp"
#include "git_diff_pivot/repeated_change_detector.hpp"
#include "git_diff_pivot/token_interner.hpp"

using git_diff_pivot::ChangedLine;
using git_diff_pivot::ChangeHunk;
using git_diff_pivot::ChangeSelection;
using git_diff_pivot::ChangeSelector;
using git_diff_pivot::DiffDocument;
using git_diff_pivot::DiffLineKind;
using git_diff_pivot::RepeatedChangeDetector;
using git_diff_pivot::RepeatedSequence;
using git_diff_pivot::RepeatedSequenceOccurrence;
using git_diff_pivot::TokenInterner;

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

// Builds a candidate directly (bypassing RepeatedChangeDetector) so tests can
// control gain/overlap precisely. `hunkOffsets` gives one (hunkIndex,
// offsetWithinHunk) pair per occurrence.
RepeatedSequence MakeCandidate(const DiffDocument& document, std::size_t length,
                                std::initializer_list<std::pair<std::size_t, std::size_t>> hunkOffsets) {
    RepeatedSequence sequence;
    bool first = true;
    for (const auto& [hunkIndex, offset] : hunkOffsets) {
        const auto& hunk = document.Hunks()[hunkIndex];
        if (first) {
            for (std::size_t k = 0; k < length; ++k) {
                sequence.tokens.push_back(hunk.lines[offset + k].token);
            }
            first = false;
        }
        sequence.occurrences.push_back(RepeatedSequenceOccurrence{
            .filePath = hunk.filePath,
            .startLine = hunk.lines[offset].lineNumber,
            .length = static_cast<std::uint32_t>(length),
            .hunkIndex = hunkIndex,
            .offsetWithinHunk = offset,
        });
    }
    return sequence;
}

}  // namespace

TEST_CASE("ChangeSelector returns nothing for an empty document and no candidates",
          "[change-selector]") {
    DiffDocument document;
    ChangeSelector selector;
    const auto selection = selector.Select(document, {});
    REQUIRE(selection.commonChanges.empty());
    REQUIRE(selection.uniqueLines.empty());
}

TEST_CASE("ChangeSelector accepts a non-conflicting candidate and exposes leftover lines",
          "[change-selector]") {
    TokenInterner interner;
    DiffDocument document;
    document.AddHunk(MakeHunk(interner, "FileA", {"A", "B", "X"}));
    document.AddHunk(MakeHunk(interner, "FileB", {"A", "B", "Y"}));

    auto candidate = MakeCandidate(document, 2, {{0, 0}, {1, 0}});

    ChangeSelector selector;
    const auto selection = selector.Select(document, {candidate});

    REQUIRE(selection.commonChanges.size() == 1);
    REQUIRE(selection.commonChanges.front().occurrences.size() == 2);

    // "X" (FileA line 3) and "Y" (FileB line 3) are not part of the
    // accepted candidate, so they must show up as unique lines.
    REQUIRE(selection.uniqueLines.size() == 2);
    REQUIRE(selection.uniqueLines[0].filePath == "FileA");
    REQUIRE(selection.uniqueLines[0].line.lineNumber == 3);
    REQUIRE(selection.uniqueLines[1].filePath == "FileB");
    REQUIRE(selection.uniqueLines[1].line.lineNumber == 3);
}

TEST_CASE("ChangeSelector discards candidates below configured thresholds", "[change-selector]") {
    TokenInterner interner;
    DiffDocument document;
    document.AddHunk(MakeHunk(interner, "FileA", {"A", "B"}));
    document.AddHunk(MakeHunk(interner, "FileB", {"A", "B"}));

    auto shortCandidate = MakeCandidate(document, 1, {{0, 0}, {1, 0}});  // Length 1, below threshold.

    ChangeSelector selector(/*minSequenceLength=*/2, /*minOccurrenceCount=*/2);
    const auto selection = selector.Select(document, {shortCandidate});

    REQUIRE(selection.commonChanges.empty());
    // Every line is unique since the only candidate was discarded.
    REQUIRE(selection.uniqueLines.size() == 4);
}

TEST_CASE("ChangeSelector resolves positional overlap by preferring higher gain", "[change-selector]") {
    // "A B C" (gain (3-1)*3=4) fully contains "B C" (gain (3-1)*2=2) at the
    // same 3 positions; only the higher-gain candidate should be accepted.
    TokenInterner interner;
    DiffDocument document;
    document.AddHunk(MakeHunk(interner, "FileA", {"A", "B", "C", "D"}));
    document.AddHunk(MakeHunk(interner, "FileB", {"A", "B", "C", "E"}));
    document.AddHunk(MakeHunk(interner, "FileC", {"X", "A", "B", "C"}));

    auto longCandidate = MakeCandidate(document, 3, {{0, 0}, {1, 0}, {2, 1}});
    auto shortCandidate = MakeCandidate(document, 2, {{0, 1}, {1, 1}, {2, 2}});

    ChangeSelector selector;
    const auto selection = selector.Select(document, {shortCandidate, longCandidate});

    REQUIRE(selection.commonChanges.size() == 1);
    REQUIRE(selection.commonChanges.front().tokens.size() == 3);

    // D, E, and X remain unique; everything else was claimed by "A B C".
    REQUIRE(selection.uniqueLines.size() == 3);
}

TEST_CASE("ChangeSelector accepts non-overlapping candidates from the same hunk", "[change-selector]") {
    TokenInterner interner;
    DiffDocument document;
    document.AddHunk(MakeHunk(interner, "FileA", {"A", "B", "M", "C", "D"}));
    document.AddHunk(MakeHunk(interner, "FileB", {"A", "B", "N", "C", "D"}));

    auto firstPair = MakeCandidate(document, 2, {{0, 0}, {1, 0}});   // "A B"
    auto secondPair = MakeCandidate(document, 2, {{0, 3}, {1, 3}});  // "C D"

    ChangeSelector selector;
    const auto selection = selector.Select(document, {firstPair, secondPair});

    REQUIRE(selection.commonChanges.size() == 2);
    REQUIRE(selection.uniqueLines.size() == 2);  // "M" and "N".
}

TEST_CASE("ChangeSelector integrates with RepeatedChangeDetector on the canonical fixture",
          "[change-selector][fixture]") {
    TokenInterner interner;
    DiffDocument document;
    document.AddHunk(MakeHunk(interner, "FileA", {"A", "B", "C", "D"}));
    document.AddHunk(MakeHunk(interner, "FileB", {"A", "B", "C", "E"}));
    document.AddHunk(MakeHunk(interner, "FileC", {"X", "A", "B", "C"}));

    RepeatedChangeDetector detector;
    const auto candidates = detector.Detect(document);

    ChangeSelector selector;
    const auto selection = selector.Select(document, candidates);

    REQUIRE(selection.commonChanges.size() == 1);
    REQUIRE(selection.commonChanges.front().tokens.size() == 3);
    REQUIRE(selection.commonChanges.front().occurrences.size() == 3);

    REQUIRE(selection.uniqueLines.size() == 3);
    for (const auto& unique : selection.uniqueLines) {
        REQUIRE(unique.line.token != selection.commonChanges.front().tokens[0]);
    }
}
