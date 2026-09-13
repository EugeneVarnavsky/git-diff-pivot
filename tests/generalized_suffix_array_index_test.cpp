#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

#include "git_diff_pivot/diff_document.hpp"
#include "git_diff_pivot/generalized_suffix_array_index.hpp"
#include "git_diff_pivot/token_interner.hpp"

using git_diff_pivot::ChangedLine;
using git_diff_pivot::ChangeHunk;
using git_diff_pivot::DiffDocument;
using git_diff_pivot::DiffLineKind;
using git_diff_pivot::GeneralizedSuffixArrayIndex;
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

TEST_CASE("GeneralizedSuffixArrayIndex builds one entry per token plus one separator per hunk",
          "[generalized-suffix-array]") {
    TokenInterner interner;
    DiffDocument document;
    document.AddHunk(MakeHunk(interner, "FileA", {"A", "B", "C", "D"}));
    document.AddHunk(MakeHunk(interner, "FileB", {"A", "B", "C", "E"}));

    GeneralizedSuffixArrayIndex index;
    index.Build(document);

    REQUIRE(index.SuffixArray().size() == 10);  // 4 + 1 separator, 4 + 1 separator.
    REQUIRE(index.LcpArray().size() == 10);

    // The token positions map back to the hunk/offset they came from.
    for (std::size_t offset = 0; offset < 4; ++offset) {
        REQUIRE_FALSE(index.IsSeparator(offset));
        const auto& position = index.PositionFor(offset);
        REQUIRE(position.hunkIndex == 0);
        REQUIRE(position.offsetWithinHunk == offset);
    }
    REQUIRE(index.IsSeparator(4));
    REQUIRE(index.PositionFor(4).hunkIndex == 0);
    REQUIRE(index.PositionFor(4).offsetWithinHunk == 4);

    for (std::size_t offset = 0; offset < 4; ++offset) {
        REQUIRE_FALSE(index.IsSeparator(5 + offset));
        const auto& position = index.PositionFor(5 + offset);
        REQUIRE(position.hunkIndex == 1);
        REQUIRE(position.offsetWithinHunk == offset);
    }
    REQUIRE(index.IsSeparator(9));
}

TEST_CASE("GeneralizedSuffixArrayIndex handles a document with no hunks", "[generalized-suffix-array]") {
    DiffDocument document;

    GeneralizedSuffixArrayIndex index;
    index.Build(document);

    REQUIRE(index.SuffixArray().empty());
    REQUIRE(index.LcpArray().empty());
}

TEST_CASE("GeneralizedSuffixArrayIndex never lets a common prefix bridge a hunk boundary",
          "[generalized-suffix-array]") {
    // Without per-hunk separators, the suffix starting at FileA's trailing
    // "B" would read straight into FileB's "C D", producing a fictitious
    // "B C D" sequence that only appears to match FileC's real "B C D"
    // hunk. Unique separators must cap that fictitious match at length 1.
    TokenInterner interner;
    DiffDocument document;
    document.AddHunk(MakeHunk(interner, "FileA", {"A", "B"}));
    document.AddHunk(MakeHunk(interner, "FileB", {"C", "D"}));
    document.AddHunk(MakeHunk(interner, "FileC", {"B", "C", "D"}));

    GeneralizedSuffixArrayIndex index;
    index.Build(document);

    const auto& lcpArray = index.LcpArray();
    const auto maxLcp = *std::max_element(lcpArray.begin(), lcpArray.end());

    // The only genuine repeated sequence longer than one token is none: "A",
    // "B", "C", and "D" each occur just once outside of FileC's own hunk.
    // A max LCP of 3 would mean the fictitious cross-hunk "B C D" matched
    // FileC's real "B C D".
    REQUIRE(maxLcp < 3);
}
