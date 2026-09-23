#include "git_diff_pivot/change_selector.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <map>
#include <queue>
#include <set>
#include <tuple>
#include <utility>

namespace git_diff_pivot {

namespace {

struct RankedCandidate {
    RepeatedSequence sequence;
    std::uint64_t gain{};
};

// Half-open [start, end) claims per hunk, sorted/disjoint (start -> end) for O(log n) lookup/insert.
using ClaimedIntervals = std::map<std::size_t, std::size_t>;
using ClaimedRanges = std::map<std::size_t, ClaimedIntervals>;

bool Overlaps(const ClaimedRanges& claimed, std::size_t hunkIndex, std::size_t start, std::size_t end) {
    const auto hunkIt = claimed.find(hunkIndex);
    if (hunkIt == claimed.end()) {
        return false;
    }
    const auto& intervals = hunkIt->second;
    // Disjoint & sorted by start => end is non-decreasing too; only the
    // largest-start interval below `end` can reach into [start, end).
    auto it = intervals.lower_bound(end);
    if (it == intervals.begin()) {
        return false;
    }
    --it;
    return it->second > start;
}

void Claim(ClaimedRanges& claimed, std::size_t hunkIndex, std::size_t start, std::size_t end) {
    claimed[hunkIndex].emplace(start, end);
}

// Marks free[offset]=false for offsets in [occStart, occStart+free.size())
// covered by `claimed`, walking only the intersecting intervals.
void MarkClaimedOffsets(const ClaimedRanges& claimed, std::size_t hunkIndex, std::size_t occStart,
                         std::vector<std::uint8_t>& free) {
    const auto hunkIt = claimed.find(hunkIndex);
    if (hunkIt == claimed.end()) {
        return;
    }
    const auto& intervals = hunkIt->second;
    const std::size_t occEnd = occStart + free.size();

    auto it = intervals.upper_bound(occStart);
    if (it != intervals.begin()) {
        const auto previous = std::prev(it);
        if (previous->second > occStart) {
            it = previous;
        }
    }
    for (; it != intervals.end() && it->first < occEnd; ++it) {
        const std::size_t overlapStart = std::max(it->first, occStart);
        const std::size_t overlapEnd = std::min(it->second, occEnd);
        for (std::size_t offset = overlapStart; offset < overlapEnd; ++offset) {
            free[offset - occStart] = false;
        }
    }
}

// True if `a` outranks `b`: length, gain, occurrence count desc, then position/token id asc.
// Length first keeps a long verbatim repeat whole instead of losing to a shorter, more frequent subsequence.
bool IsBetter(const RankedCandidate& a, const RankedCandidate& b) {
    assert(!a.sequence.occurrences.empty() && !b.sequence.occurrences.empty());
    if (a.sequence.tokens.size() != b.sequence.tokens.size()) {
        return a.sequence.tokens.size() > b.sequence.tokens.size();
    }
    if (a.gain != b.gain) {
        return a.gain > b.gain;
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
}

// priority_queue keeps the "largest" on top; reversing IsBetter makes that the best candidate.
struct WorseThan {
    bool operator()(const RankedCandidate& a, const RankedCandidate& b) const { return IsBetter(b, a); }
};

using PendingQueue = std::priority_queue<RankedCandidate, std::vector<RankedCandidate>, WorseThan>;

RankedCandidate PopBest(PendingQueue& pending) {
    // Safe: popped right after, so moving from the const top() first is fine.
    RankedCandidate best = std::move(const_cast<RankedCandidate&>(pending.top()));
    pending.pop();
    return best;
}

// A candidate's document position (hunk/offset/length per occurrence, sorted),
// used to drop a split result that duplicates an already-seen candidate.
using CandidateSignature = std::vector<std::tuple<std::size_t, std::size_t, std::size_t>>;

CandidateSignature SignatureOf(const RepeatedSequence& sequence) {
    CandidateSignature signature;
    signature.reserve(sequence.occurrences.size());
    for (const auto& occurrence : sequence.occurrences) {
        signature.emplace_back(occurrence.hunkIndex, occurrence.offsetWithinHunk, occurrence.length);
    }
    std::sort(signature.begin(), signature.end());
    return signature;
}

// Maximal runs of tokens free across a consistent occurrence subset. One run
// covering every token/occurrence means `sequence` had no conflict at all.
// Greedy left-to-right: commits to whichever subset is free at a run's start
// and extends it; doesn't search for a higher-gain alternative split.
std::vector<RankedCandidate> FreeRuns(const DiffDocument& document, const RepeatedSequence& sequence,
                                       const ClaimedRanges& claimed) {
    const std::size_t length = sequence.tokens.size();
    const std::size_t occurrenceCount = sequence.occurrences.size();

    // freePerOccurrence[i][offset]: occurrence i still owns token `offset`.
    std::vector<std::vector<std::uint8_t>> freePerOccurrence(occurrenceCount, std::vector<std::uint8_t>(length, true));
    for (std::size_t i = 0; i < occurrenceCount; ++i) {
        const auto& occurrence = sequence.occurrences[i];
        MarkClaimedOffsets(claimed, occurrence.hunkIndex, occurrence.offsetWithinHunk, freePerOccurrence[i]);
    }

    std::vector<RankedCandidate> runs;
    std::size_t runStart = 0;
    while (runStart < length) {
        std::vector<std::size_t> subset;
        for (std::size_t i = 0; i < occurrenceCount; ++i) {
            if (freePerOccurrence[i][runStart]) {
                subset.push_back(i);
            }
        }
        if (subset.empty()) {
            ++runStart;
            continue;
        }

        // Extend while exactly `subset` stays free, for a shared maximal span.
        std::size_t runEnd = runStart + 1;
        while (runEnd < length && std::all_of(subset.begin(), subset.end(), [&](std::size_t i) {
                   return freePerOccurrence[i][runEnd] != 0;
               })) {
            ++runEnd;
        }

        const std::size_t runLength = runEnd - runStart;
        RepeatedSequence run;
        run.tokens.assign(sequence.tokens.begin() + static_cast<std::ptrdiff_t>(runStart),
                           sequence.tokens.begin() + static_cast<std::ptrdiff_t>(runEnd));
        run.occurrences.reserve(subset.size());
        for (const auto occIndex : subset) {
            const auto& occurrence = sequence.occurrences[occIndex];
            const auto& hunk = document.Hunks()[occurrence.hunkIndex];
            const auto newOffset = occurrence.offsetWithinHunk + runStart;
            run.occurrences.push_back(RepeatedSequenceOccurrence{
                .filePath = occurrence.filePath,
                .startLine = hunk.lines[newOffset].lineNumber,
                .length = static_cast<std::uint32_t>(runLength),
                .hunkIndex = occurrence.hunkIndex,
                .offsetWithinHunk = newOffset,
            });
        }
        const auto gain = static_cast<std::uint64_t>(run.occurrences.size() - 1) * static_cast<std::uint64_t>(runLength);
        runs.push_back(RankedCandidate{.sequence = std::move(run), .gain = gain});

        runStart = runEnd;
    }
    return runs;
}

// True when FreeRuns found no conflict: one run spans the whole sequence.
bool IsFullyFree(const std::vector<RankedCandidate>& runs, const RepeatedSequence& sequence) {
    return runs.size() == 1 && runs.front().sequence.tokens.size() == sequence.tokens.size() &&
           runs.front().sequence.occurrences.size() == sequence.occurrences.size();
}

}  // namespace

ChangeSelection ChangeSelector::Select(const DiffDocument& document, std::vector<RepeatedSequence> candidates) const {
    std::set<CandidateSignature> queuedSignatures;
    PendingQueue pending;
    const auto enqueue = [&](RankedCandidate candidate) {
        if (candidate.sequence.tokens.size() < minSequenceLength_ ||
            candidate.sequence.occurrences.size() < minOccurrenceCount_) {
            return;
        }
        if (!queuedSignatures.insert(SignatureOf(candidate.sequence)).second) {
            return;  // Already queued (or resolved) an identical candidate.
        }
        pending.push(std::move(candidate));
    };

    for (auto& candidate : candidates) {
        const auto occurrenceCount = candidate.occurrences.size();
        const auto length = candidate.tokens.size();
        const auto gain = static_cast<std::uint64_t>(occurrenceCount - 1) * static_cast<std::uint64_t>(length);
        enqueue(RankedCandidate{.sequence = std::move(candidate), .gain = gain});
    }

    ClaimedRanges claimed;
    ChangeSelection selection;
    while (!pending.empty()) {
        RankedCandidate best = PopBest(pending);

        auto runs = FreeRuns(document, best.sequence, claimed);
        if (IsFullyFree(runs, best.sequence)) {
            for (const auto& occurrence : best.sequence.occurrences) {
                Claim(claimed, occurrence.hunkIndex, occurrence.offsetWithinHunk,
                      occurrence.offsetWithinHunk + occurrence.length);
            }
            selection.commonChanges.push_back(std::move(best.sequence));
            continue;
        }

        for (auto& run : runs) {
            enqueue(std::move(run));
        }
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
