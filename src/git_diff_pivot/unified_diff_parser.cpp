#include "git_diff_pivot/unified_diff_parser.hpp"

#include <cstdlib>
#include <optional>
#include <string_view>

namespace git_diff_pivot {

namespace {

constexpr std::string_view kDiffGitPrefix = "diff --git ";
constexpr std::string_view kOldFilePrefix = "--- ";
constexpr std::string_view kNewFilePrefix = "+++ ";
constexpr std::string_view kHunkPrefix = "@@ ";
constexpr std::string_view kBinaryMarker = "Binary files ";
constexpr std::string_view kDevNull = "/dev/null";

// Strips a leading "a/" or "b/" prefix used by default git diff paths, and
// drops any trailing tab-separated metadata (e.g. a timestamp).
[[nodiscard]] std::string StripPathPrefix(std::string_view rawPath) {
    if (const auto tabPos = rawPath.find('\t'); tabPos != std::string_view::npos) {
        rawPath = rawPath.substr(0, tabPos);
    }
    if (rawPath.size() > 2 && (rawPath[0] == 'a' || rawPath[0] == 'b') && rawPath[1] == '/') {
        rawPath.remove_prefix(2);
    }
    return std::string(rawPath);
}

struct HunkHeader {
    std::uint32_t oldStart{};
    std::uint32_t newStart{};
};

// Parses the starting line numbers out of a "@@ -oldStart,oldCount +newStart,newCount @@" header.
[[nodiscard]] std::optional<HunkHeader> ParseHunkHeader(std::string_view line) {
    std::string_view rest = line.substr(kHunkPrefix.size());
    const auto midSpace = rest.find(' ');
    if (midSpace == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view oldSpec = rest.substr(0, midSpace);
    std::string_view afterOld = rest.substr(midSpace + 1);
    const auto endSpace = afterOld.find(' ');
    const std::string_view newSpec = endSpace == std::string_view::npos ? afterOld : afterOld.substr(0, endSpace);

    if (oldSpec.empty() || oldSpec.front() != '-' || newSpec.empty() || newSpec.front() != '+') {
        return std::nullopt;
    }

    const auto parseStart = [](std::string_view spec) -> std::uint32_t {
        spec.remove_prefix(1);  // Drop the leading '-' or '+'.
        const auto commaPos = spec.find(',');
        const std::string_view startPart = commaPos == std::string_view::npos ? spec : spec.substr(0, commaPos);
        return static_cast<std::uint32_t>(std::strtoul(std::string(startPart).c_str(), nullptr, 10));
    };

    return HunkHeader{.oldStart = parseStart(oldSpec), .newStart = parseStart(newSpec)};
}

// Folds the diff operation into the interned text so that a deletion and an
// addition with identical content remain distinct tokens.
[[nodiscard]] TokenId InternChangedLine(TokenInterner& interner, std::string_view content) {
    return interner.Intern(content);
}

}  // namespace

DiffDocument UnifiedDiffParser::Parse(const std::vector<std::string>& lines, TokenInterner& interner) {
    DiffDocument document;

    std::string currentFilePath;
    std::string pendingOldPath;
    ChangeHunk currentHunk;
    bool isBinaryEntry = false;
    bool beforeFirstHunk = true;
    std::uint32_t oldLine = 0;
    std::uint32_t newLine = 0;

    const auto flushHunk = [&]() {
        if (!currentHunk.lines.empty()) {
            document.AddHunk(std::move(currentHunk));
        }
        currentHunk = ChangeHunk{};
        currentHunk.filePath = currentFilePath;
    };

    for (const std::string& rawLine : lines) {
        const std::string_view line = rawLine;

        if (line.rfind(kDiffGitPrefix, 0) == 0) {
            flushHunk();
            currentFilePath.clear();
            currentHunk.filePath.clear();
            pendingOldPath.clear();
            isBinaryEntry = false;
            beforeFirstHunk = true;
            continue;
        }

        if (isBinaryEntry) {
            continue;  // Skip binary payload/metadata lines until the next file entry.
        }

        if (line.rfind(kBinaryMarker, 0) == 0) {
            flushHunk();
            isBinaryEntry = true;
            continue;
        }

        if (beforeFirstHunk) {
            if (line.rfind(kOldFilePrefix, 0) == 0) {
                pendingOldPath = StripPathPrefix(line.substr(kOldFilePrefix.size()));
                continue;
            }
            if (line.rfind(kNewFilePrefix, 0) == 0) {
                const std::string newPath = StripPathPrefix(line.substr(kNewFilePrefix.size()));
                currentFilePath = (newPath == kDevNull) ? pendingOldPath : newPath;
                currentHunk.filePath = currentFilePath;
                continue;
            }
            if (line.rfind(kHunkPrefix, 0) == 0) {
                const auto header = ParseHunkHeader(line);
                if (header) {
                    oldLine = header->oldStart;
                    newLine = header->newStart;
                    beforeFirstHunk = false;
                }
                continue;
            }
            continue;  // Other file-entry metadata (index/mode/rename lines).
        }

        if (line.rfind(kHunkPrefix, 0) == 0) {
            const auto header = ParseHunkHeader(line);
            if (header) {
                flushHunk();
                oldLine = header->oldStart;
                newLine = header->newStart;
            }
            continue;
        }

        if (line.empty() || line.front() == ' ') {
            flushHunk();
            ++oldLine;
            ++newLine;
            continue;
        }

        if (currentHunk.lines.empty()) {
            currentHunk.anchorOldLine = oldLine;
            currentHunk.anchorNewLine = newLine;
        }

        switch (line.front()) {
            case '-':
                currentHunk.lines.push_back(ChangedLine{
                    .token = InternChangedLine(interner, line),
                    .lineNumber = oldLine,
                    .kind = DiffLineKind::Removed,
                });
                ++oldLine;
                break;
            case '+':
                currentHunk.lines.push_back(ChangedLine{
                    .token = InternChangedLine(interner, line),
                    .lineNumber = newLine,
                    .kind = DiffLineKind::Added,
                });
                ++newLine;
                break;
            case '\\':
                break;  // e.g. "\ No newline at end of file"; not a content or boundary line.
            default:
                flushHunk();  // Unexpected marker: treat conservatively as a hunk boundary.
                break;
        }
    }

    flushHunk();
    return document;
}

}  // namespace git_diff_pivot
