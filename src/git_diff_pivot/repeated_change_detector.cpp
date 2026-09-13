#include "git_diff_pivot/repeated_change_detector.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "git_diff_pivot/generalized_suffix_array_index.hpp"

namespace git_diff_pivot {

namespace {

// A maximal set of suffix-array entries [startIndex, endIndex] that all
// share a common prefix of exactly `height` tokens.
struct LcpGroup {
    std::size_t height{};
    std::size_t startIndex{};
    std::size_t endIndex{};
};

// Extracts every maximal LCP-interval group from `lcpArray` with a single
// left-to-right pass and a monotonic stack, in O(n) time.
// Each group corresponds to one internal node of the implicit suffix tree:
// the maximal run of suffix-array entries that agree on their first
// `height` tokens, bounded on both sides by a strictly smaller LCP value.
std::vector<LcpGroup> ExtractLcpGroups(const std::vector<std::int32_t>& lcpArray) {
    struct StackEntry {
        std::size_t height;
        std::size_t startIndex;
    };

    std::vector<LcpGroup> groups;
    std::vector<StackEntry> stack{StackEntry{0, 0}};

    const std::size_t n = lcpArray.size();
    for (std::size_t i = 1; i < n; ++i) {
        const auto currentHeight = static_cast<std::size_t>(lcpArray[i]);
        std::size_t start = i - 1;
        while (stack.back().height > currentHeight) {
            const StackEntry top = stack.back();
            stack.pop_back();
            if (top.height > 0) {
                groups.push_back(LcpGroup{.height = top.height, .startIndex = top.startIndex, .endIndex = i - 1});
            }
            start = top.startIndex;
        }
        if (stack.back().height < currentHeight) {
            stack.push_back(StackEntry{currentHeight, start});
        }
    }
    while (!stack.empty()) {
        const StackEntry top = stack.back();
        stack.pop_back();
        if (top.height > 0) {
            groups.push_back(LcpGroup{.height = top.height, .startIndex = top.startIndex, .endIndex = n - 1});
        }
    }
    return groups;
}

}  // namespace

std::vector<RepeatedSequence> RepeatedChangeDetector::Detect(const DiffDocument& document) const {
    GeneralizedSuffixArrayIndex index;
    index.Build(document);

    const auto& suffixArray = index.SuffixArray();
    const auto& hunks = document.Hunks();

    std::vector<RepeatedSequence> results;
    for (const auto& group : ExtractLcpGroups(index.LcpArray())) {
        if (group.height < minSequenceLength_ || group.endIndex - group.startIndex + 1 < 2) {
            continue;
        }

        // (hunkIndex, offsetWithinHunk) for every occurrence, used to build
        // the result and to order occurrences by original diff position.
        std::vector<HunkTokenPosition> memberPositions;
        memberPositions.reserve(group.endIndex - group.startIndex + 1);
        for (std::size_t saIndex = group.startIndex; saIndex <= group.endIndex; ++saIndex) {
            memberPositions.push_back(index.PositionFor(static_cast<std::size_t>(suffixArray[saIndex])));
        }
        std::sort(memberPositions.begin(), memberPositions.end(), [](const auto& a, const auto& b) {
            return a.hunkIndex != b.hunkIndex ? a.hunkIndex < b.hunkIndex : a.offsetWithinHunk < b.offsetWithinHunk;
        });

        RepeatedSequence sequence;
        const auto& firstHunk = hunks[memberPositions.front().hunkIndex];
        sequence.tokens.reserve(group.height);
        for (std::size_t offset = 0; offset < group.height; ++offset) {
            sequence.tokens.push_back(firstHunk.lines[memberPositions.front().offsetWithinHunk + offset].token);
        }

        sequence.occurrences.reserve(memberPositions.size());
        for (const auto& position : memberPositions) {
            const auto& hunk = hunks[position.hunkIndex];
            sequence.occurrences.push_back(RepeatedSequenceOccurrence{
                .filePath = hunk.filePath,
                .startLine = hunk.lines[position.offsetWithinHunk].lineNumber,
                .length = static_cast<std::uint32_t>(group.height),
                .hunkIndex = position.hunkIndex,
                .offsetWithinHunk = position.offsetWithinHunk,
            });
        }

        results.push_back(std::move(sequence));
    }

    // Deterministic candidate ordering: by first occurrence's position in
    // the original diff, then by descending length, then by first token id.
    std::sort(results.begin(), results.end(), [](const RepeatedSequence& a, const RepeatedSequence& b) {
        const auto& firstA = a.occurrences.front();
        const auto& firstB = b.occurrences.front();
        if (firstA.filePath != firstB.filePath) {
            return firstA.filePath < firstB.filePath;
        }
        if (firstA.startLine != firstB.startLine) {
            return firstA.startLine < firstB.startLine;
        }
        if (a.tokens.size() != b.tokens.size()) {
            return a.tokens.size() > b.tokens.size();
        }
        return a.tokens.front() < b.tokens.front();
    });

    return results;
}

}  // namespace git_diff_pivot
