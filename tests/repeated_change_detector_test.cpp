#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "git_diff_pivot/diff_document.hpp"
#include "git_diff_pivot/repeated_change_detector.hpp"
#include "git_diff_pivot/token_interner.hpp"

using git_diff_pivot::ChangedLine;
using git_diff_pivot::ChangeHunk;
using git_diff_pivot::DiffDocument;
using git_diff_pivot::DiffLineKind;
using git_diff_pivot::RepeatedChangeDetector;
using git_diff_pivot::RepeatedSequence;
using git_diff_pivot::RepeatedSequenceOccurrence;
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

// A normalized, order-independent view of a RepeatedSequence used to
// compare results between the production detector and the brute-force
// oracle below, regardless of candidate/occurrence ordering.
using NormalizedOccurrence = std::tuple<std::string, std::uint32_t, std::uint32_t>;
using NormalizedSequence = std::pair<std::vector<git_diff_pivot::TokenId>, std::vector<NormalizedOccurrence>>;

NormalizedSequence Normalize(const RepeatedSequence& sequence) {
    std::vector<NormalizedOccurrence> occurrences;
    occurrences.reserve(sequence.occurrences.size());
    for (const auto& occurrence : sequence.occurrences) {
        occurrences.emplace_back(occurrence.filePath, occurrence.startLine, occurrence.length);
    }
    std::sort(occurrences.begin(), occurrences.end());
    return {sequence.tokens, std::move(occurrences)};
}

std::set<NormalizedSequence> NormalizeAll(const std::vector<RepeatedSequence>& sequences) {
    std::set<NormalizedSequence> normalized;
    for (const auto& sequence : sequences) {
        normalized.insert(Normalize(sequence));
    }
    return normalized;
}

// Brute-force oracle: for a tiny DiffDocument, independently finds every
// maximal repeated token sequence by directly comparing substrings, without
// any suffix-array machinery. A candidate (content, occurrence positions) is
// reported only if it cannot be uniformly extended by one more token while
// keeping the exact same occurrence positions (that longer extension would
// be reported instead), mirroring what the LCP-interval sweep produces.
std::set<NormalizedSequence> BruteForceDetect(const DiffDocument& document, std::size_t minSequenceLength) {
    struct Position {
        std::size_t hunkIndex;
        std::size_t offset;
    };

    const auto& hunks = document.Hunks();
    std::size_t maxLength = 0;
    for (const auto& hunk : hunks) {
        maxLength = std::max(maxLength, hunk.lines.size());
    }

    std::set<NormalizedSequence> accepted;
    for (std::size_t length = maxLength; length >= 1 && length >= minSequenceLength; --length) {
        std::map<std::vector<git_diff_pivot::TokenId>, std::vector<Position>> groups;
        for (std::size_t hunkIndex = 0; hunkIndex < hunks.size(); ++hunkIndex) {
            const auto& hunk = hunks[hunkIndex];
            if (hunk.lines.size() < length) {
                continue;
            }
            for (std::size_t offset = 0; offset + length <= hunk.lines.size(); ++offset) {
                std::vector<git_diff_pivot::TokenId> content;
                content.reserve(length);
                for (std::size_t k = 0; k < length; ++k) {
                    content.push_back(hunk.lines[offset + k].token);
                }
                groups[content].push_back(Position{hunkIndex, offset});
            }
        }

        for (const auto& [content, positions] : groups) {
            if (positions.size() < 2) {
                continue;
            }

            bool allExtendSame = true;
            std::optional<git_diff_pivot::TokenId> commonNext;
            for (const auto& position : positions) {
                const auto& hunk = hunks[position.hunkIndex];
                const auto nextOffset = position.offset + length;
                if (nextOffset >= hunk.lines.size()) {
                    allExtendSame = false;
                    break;
                }
                const auto nextToken = hunk.lines[nextOffset].token;
                if (!commonNext) {
                    commonNext = nextToken;
                } else if (*commonNext != nextToken) {
                    allExtendSame = false;
                    break;
                }
            }
            if (allExtendSame) {
                continue;  // Reported at `length + 1` instead.
            }

            std::vector<NormalizedOccurrence> occurrences;
            occurrences.reserve(positions.size());
            for (const auto& position : positions) {
                const auto& hunk = hunks[position.hunkIndex];
                occurrences.emplace_back(hunk.filePath, hunk.lines[position.offset].lineNumber,
                                          static_cast<std::uint32_t>(length));
            }
            std::sort(occurrences.begin(), occurrences.end());
            accepted.insert({content, std::move(occurrences)});
        }
    }
    return accepted;
}

}  // namespace

TEST_CASE("RepeatedChangeDetector returns nothing for a document with no hunks", "[repeated-change-detector]") {
    DiffDocument document;
    RepeatedChangeDetector detector;
    REQUIRE(detector.Detect(document).empty());
}

TEST_CASE("RepeatedChangeDetector finds no repeats when every hunk is distinct",
          "[repeated-change-detector]") {
    TokenInterner interner;
    DiffDocument document;
    document.AddHunk(MakeHunk(interner, "FileA", {"A", "B"}));
    document.AddHunk(MakeHunk(interner, "FileB", {"C", "D"}));

    RepeatedChangeDetector detector;
    REQUIRE(detector.Detect(document).empty());
}

TEST_CASE("RepeatedChangeDetector finds repeats within a single hunk", "[repeated-change-detector]") {
    TokenInterner interner;
    DiffDocument document;
    document.AddHunk(MakeHunk(interner, "FileA", {"A", "B", "X", "A", "B"}));

    RepeatedChangeDetector detector;
    const auto results = detector.Detect(document);

    const auto normalized = NormalizeAll(results);
    REQUIRE(normalized.count(Normalize(RepeatedSequence{
                .tokens = {interner.Intern("A"), interner.Intern("B")},
                .occurrences =
                    {
                        RepeatedSequenceOccurrence{.filePath = "FileA", .startLine = 1, .length = 2},
                        RepeatedSequenceOccurrence{.filePath = "FileA", .startLine = 4, .length = 2},
                    },
            })) == 1);
}

TEST_CASE("RepeatedChangeDetector handles repeated-token runs correctly", "[repeated-change-detector]") {
    // A run of three identical tokens: "A" repeats 3 times, "A A" repeats
    // twice (positions 0 and 1; position 2 has no room to extend), and
    // "A A A" occurs only once, so it is not a repeat.
    TokenInterner interner;
    DiffDocument document;
    document.AddHunk(MakeHunk(interner, "FileA", {"A", "A", "A"}));

    RepeatedChangeDetector detector;
    const auto normalized = NormalizeAll(detector.Detect(document));

    REQUIRE(normalized.size() == 2);
    REQUIRE(normalized.count(Normalize(RepeatedSequence{
                .tokens = {interner.Intern("A")},
                .occurrences =
                    {
                        RepeatedSequenceOccurrence{.filePath = "FileA", .startLine = 1, .length = 1},
                        RepeatedSequenceOccurrence{.filePath = "FileA", .startLine = 2, .length = 1},
                        RepeatedSequenceOccurrence{.filePath = "FileA", .startLine = 3, .length = 1},
                    },
            })) == 1);
    REQUIRE(normalized.count(Normalize(RepeatedSequence{
                .tokens = {interner.Intern("A"), interner.Intern("A")},
                .occurrences =
                    {
                        RepeatedSequenceOccurrence{.filePath = "FileA", .startLine = 1, .length = 2},
                        RepeatedSequenceOccurrence{.filePath = "FileA", .startLine = 2, .length = 2},
                    },
            })) == 1);
}

TEST_CASE("RepeatedChangeDetector finds the canonical File A/B/C -> A B C repeated sequence",
          "[repeated-change-detector][fixture]") {
    TokenInterner interner;
    DiffDocument document;
    document.AddHunk(MakeHunk(interner, "FileA", {"A", "B", "C", "D"}));
    document.AddHunk(MakeHunk(interner, "FileB", {"A", "B", "C", "E"}));
    document.AddHunk(MakeHunk(interner, "FileC", {"X", "A", "B", "C"}));

    RepeatedChangeDetector detector(/*minSequenceLength=*/3);
    const auto results = detector.Detect(document);

    REQUIRE(results.size() == 1);
    const auto& sequence = results.front();
    REQUIRE(sequence.tokens == std::vector<git_diff_pivot::TokenId>{
                                    interner.Intern("A"), interner.Intern("B"), interner.Intern("C")});

    const auto normalized = Normalize(sequence);
    const auto expected = Normalize(RepeatedSequence{
        .tokens = sequence.tokens,
        .occurrences =
            {
                RepeatedSequenceOccurrence{.filePath = "FileA", .startLine = 1, .length = 3},
                RepeatedSequenceOccurrence{.filePath = "FileB", .startLine = 1, .length = 3},
                RepeatedSequenceOccurrence{.filePath = "FileC", .startLine = 2, .length = 3},
            },
    });
    REQUIRE(normalized == expected);
}

TEST_CASE("RepeatedChangeDetector matches a brute-force oracle on tiny fixtures",
          "[repeated-change-detector][oracle]") {
    TokenInterner interner;

    SECTION("canonical File A/B/C fixture") {
        DiffDocument document;
        document.AddHunk(MakeHunk(interner, "FileA", {"A", "B", "C", "D"}));
        document.AddHunk(MakeHunk(interner, "FileB", {"A", "B", "C", "E"}));
        document.AddHunk(MakeHunk(interner, "FileC", {"X", "A", "B", "C"}));

        RepeatedChangeDetector detector;
        REQUIRE(NormalizeAll(detector.Detect(document)) == BruteForceDetect(document, 1));
    }

    SECTION("repeated-token run within one hunk") {
        DiffDocument document;
        document.AddHunk(MakeHunk(interner, "FileA", {"A", "A", "A"}));

        RepeatedChangeDetector detector;
        REQUIRE(NormalizeAll(detector.Detect(document)) == BruteForceDetect(document, 1));
    }

    SECTION("mixed overlapping repeats across many hunks") {
        DiffDocument document;
        document.AddHunk(MakeHunk(interner, "FileA", {"A", "B", "C", "D"}));
        document.AddHunk(MakeHunk(interner, "FileB", {"B", "C", "D", "E"}));
        document.AddHunk(MakeHunk(interner, "FileC", {"X", "B", "C", "D", "Y"}));
        document.AddHunk(MakeHunk(interner, "FileD", {"Z", "Z", "B", "C"}));

        RepeatedChangeDetector detector;
        REQUIRE(NormalizeAll(detector.Detect(document)) == BruteForceDetect(document, 1));
    }

    SECTION("no repeats at all") {
        DiffDocument document;
        document.AddHunk(MakeHunk(interner, "FileA", {"A", "B"}));
        document.AddHunk(MakeHunk(interner, "FileB", {"C", "D"}));

        RepeatedChangeDetector detector;
        REQUIRE(NormalizeAll(detector.Detect(document)) == BruteForceDetect(document, 1));
    }
}
