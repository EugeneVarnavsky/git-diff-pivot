#include "git_diff_pivot/change_selector.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <utility>

namespace git_diff_pivot {

namespace {

struct RankedCandidate {
    RepeatedSequence sequence;
    std::uint64_t gain{};
};

// Half-open [start, end) token-offset ranges already claimed, keyed by hunk.
using ClaimedRanges = std::map<std::size_t, std::vector<std::pair<std::size_t, std::size_t>>>;

bool Overlaps(const ClaimedRanges& claimed, std::size_t hunkIndex, std::size_t start, std::size_t end) {
    const auto it = claimed.find(hunkIndex);
    if (it == claimed.end()) {
        return false;
    }
    for (const auto& [claimedStart, claimedEnd] : it->second) {
        if (start < claimedEnd && claimedStart < end) {
            return true;
        }
    }
    return false;
}

}  // namespace

ChangeSelection ChangeSelector::Select(const DiffDocument& document, std::vector<RepeatedSequence> candidates) const {
    std::vector<RankedCandidate> ranked;
    for (auto& candidate : candidates) {
        const auto length = candidate.tokens.size();
        const auto occurrenceCount = candidate.occurrences.size();
        if (length < minSequenceLength_ || occurrenceCount < minOccurrenceCount_) {
            continue;
        }
        const auto gain = static_cast<std::uint64_t>(occurrenceCount - 1) * static_cast<std::uint64_t>(length);
        ranked.push_back(RankedCandidate{.sequence = std::move(candidate), .gain = gain});
    }

    // Deterministic tie-breaks: gain, length, occurrence
    // count descending; first occurrence position and first token id ascending.
    std::sort(ranked.begin(), ranked.end(), [](const RankedCandidate& a, const RankedCandidate& b) {
        if (a.gain != b.gain) {
            return a.gain > b.gain;
        }
        if (a.sequence.tokens.size() != b.sequence.tokens.size()) {
            return a.sequence.tokens.size() > b.sequence.tokens.size();
        }
        if (a.sequence.occurrences.size() != b.sequence.occurrences.size()) {
            return a.sequence.occurrences.size() > b.sequence.occurrences.size();
        }
        const auto& firstA = a.sequence.occurrences.front();
        const auto& firstB = b.sequence.occurrences.front();
        if (firstA.filePath != firstB.filePath) {
            return firstA.filePath < firstB.filePath;
        }
        if (firstA.startLine != firstB.startLine) {
            return firstA.startLine < firstB.startLine;
        }
        return a.sequence.tokens.front() < b.sequence.tokens.front();
    });

    ClaimedRanges claimed;
    ChangeSelection selection;
    for (auto& candidate : ranked) {
        const bool conflict =
            std::any_of(candidate.sequence.occurrences.begin(), candidate.sequence.occurrences.end(),
                        [&](const RepeatedSequenceOccurrence& occurrence) {
                            return Overlaps(claimed, occurrence.hunkIndex, occurrence.offsetWithinHunk,
                                             occurrence.offsetWithinHunk + occurrence.length);
                        });
        if (conflict) {
            continue;
        }

        for (const auto& occurrence : candidate.sequence.occurrences) {
            claimed[occurrence.hunkIndex].emplace_back(occurrence.offsetWithinHunk,
                                                        occurrence.offsetWithinHunk + occurrence.length);
        }
        selection.commonChanges.push_back(std::move(candidate.sequence));
    }

    const auto& hunks = document.Hunks();
    for (std::size_t hunkIndex = 0; hunkIndex < hunks.size(); ++hunkIndex) {
        const auto& hunk = hunks[hunkIndex];
        for (std::size_t offset = 0; offset < hunk.lines.size(); ++offset) {
            if (!Overlaps(claimed, hunkIndex, offset, offset + 1)) {
                selection.uniqueLines.push_back(UniqueLine{.filePath = hunk.filePath,
                                                            .line = hunk.lines[offset],
                                                            .hunkIndex = hunkIndex,
                                                            .hunkAnchorOldLine = hunk.anchorOldLine,
                                                            .hunkAnchorNewLine = hunk.anchorNewLine});
            }
        }
    }

    return selection;
}

}  // namespace git_diff_pivot
