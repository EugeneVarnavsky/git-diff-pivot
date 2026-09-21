#include "git_diff_pivot/output_renderer.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace git_diff_pivot {

namespace {

// One item together with the file path it was grouped under.
template <typename TItem>
struct FilePathGroup {
    std::string filePath;
    std::vector<const TItem*> items;
};

// Groups items that each carry a `filePath` member by that file, preserving
// each file's first-seen order even when its items aren't contiguous (e.g.
// a common change's occurrences interleave files in detection order).
template <typename TItem>
std::vector<FilePathGroup<TItem>> GroupByFilePath(const std::vector<TItem>& items) {
    std::vector<FilePathGroup<TItem>> groups;
    std::unordered_map<std::string, std::size_t> indexByFile;
    for (const auto& item : items) {
        const auto [it, inserted] = indexByFile.try_emplace(item.filePath, groups.size());
        if (inserted) {
            groups.push_back(FilePathGroup<TItem>{.filePath = item.filePath, .items = {}});
        }
        groups[it->second].items.push_back(&item);
    }
    return groups;
}

// One line belonging to a file group, tagged with its originating hunk so
// runs from different original hunks can be told apart when rendering.
struct FileGroupLine {
    std::size_t hunkIndex{};
    const ChangedLine* line{};
    std::uint32_t hunkAnchorOldLine{};
    std::uint32_t hunkAnchorNewLine{};
};

// One file's unique lines, in original diff order.
struct FileGroup {
    std::string filePath;
    std::vector<FileGroupLine> lines;
};

// Groups unique lines by file, preserving each file's first-seen order.
std::vector<FileGroup> GroupByFile(const std::vector<UniqueLine>& uniqueLines) {
    std::vector<FileGroup> groups;
    for (const auto& keyedGroup : GroupByFilePath(uniqueLines)) {
        FileGroup group{.filePath = keyedGroup.filePath, .lines = {}};
        group.lines.reserve(keyedGroup.items.size());
        for (const auto* unique : keyedGroup.items) {
            group.lines.push_back(FileGroupLine{.hunkIndex = unique->hunkIndex,
                                                 .line = &unique->line,
                                                 .hunkAnchorOldLine = unique->hunkAnchorOldLine,
                                                 .hunkAnchorNewLine = unique->hunkAnchorNewLine});
        }
        groups.push_back(std::move(group));
    }
    return groups;
}

// One run of consecutive unique lines from the same original hunk, ready to
// render as a single hunk with a git-style "@@ -old +new @@" header.
struct RenderHunk {
    std::uint32_t oldStart{};
    std::uint32_t oldCount{};
    std::uint32_t newStart{};
    std::uint32_t newCount{};
    std::vector<std::string_view> lineTexts;
};

// Splits a file group's lines into per-original-hunk runs (a new hunk starts
// whenever hunkIndex changes) and computes each run's git-style line range.
// A side's start/count come from that side's own lines when present;
// otherwise the start falls back to the originating hunk's anchor line and
// the count stays 0, matching how git reports pure insertions/deletions.
std::vector<RenderHunk> BuildHunks(const std::vector<FileGroupLine>& lines, const TokenInterner& interner) {
    std::vector<RenderHunk> hunks;
    std::optional<std::size_t> previousHunkIndex;
    for (const auto& entry : lines) {
        if (!previousHunkIndex.has_value() || *previousHunkIndex != entry.hunkIndex) {
            hunks.push_back(RenderHunk{.oldStart = entry.hunkAnchorOldLine,
                                       .oldCount = 0,
                                       .newStart = entry.hunkAnchorNewLine,
                                       .newCount = 0,
                                       .lineTexts = {}});
        }
        previousHunkIndex = entry.hunkIndex;
        auto& hunk = hunks.back();
        if (entry.line->kind == DiffLineKind::Removed) {
            if (hunk.oldCount == 0) {
                hunk.oldStart = entry.line->lineNumber;
            }
            ++hunk.oldCount;
        } else {
            if (hunk.newCount == 0) {
                hunk.newStart = entry.line->lineNumber;
            }
            ++hunk.newCount;
        }
        hunk.lineTexts.push_back(interner.TextFor(entry.line->token));
    }
    return hunks;
}

// Formats a hunk's line range as a git-style "@@ -oldStart,oldCount
// +newStart,newCount @@" header, omitting the ",count" part when it's 1 (as
// git itself does).
std::string FormatHunkHeader(const RenderHunk& hunk) {
    std::ostringstream out;
    out << "@@ -" << hunk.oldStart;
    if (hunk.oldCount != 1) {
        out << ',' << hunk.oldCount;
    }
    out << " +" << hunk.newStart;
    if (hunk.newCount != 1) {
        out << ',' << hunk.newCount;
    }
    out << " @@";
    return out.str();
}

// One file's occurrence start lines, in first-seen order.
struct FileOccurrences {
    std::string filePath;
    std::vector<std::uint32_t> startLines;
};

// Groups a common change's occurrences by file, preserving each file's
// first-seen order, so text/markdown output can print the path once per
// file followed by every line it occurs on.
std::vector<FileOccurrences> GroupOccurrencesByFile(
    const std::vector<RepeatedSequenceOccurrence>& occurrences) {
    std::vector<FileOccurrences> groups;
    for (const auto& keyedGroup : GroupByFilePath(occurrences)) {
        FileOccurrences group{.filePath = keyedGroup.filePath, .startLines = {}};
        group.startLines.reserve(keyedGroup.items.size());
        for (const auto* occurrence : keyedGroup.items) {
            group.startLines.push_back(occurrence->startLine);
        }
        groups.push_back(std::move(group));
    }
    return groups;
}

// Writes `values` comma-separated, with no leading/trailing separator.
void WriteCommaSeparated(std::ostream& out, const std::vector<std::uint32_t>& values) {
    for (std::size_t k = 0; k < values.size(); ++k) {
        out << (k == 0 ? "" : ",") << values[k];
    }
}

// A changed line's interned text carries its diff operation as a leading
// '+'/'-' marker. A line is blank if nothing but whitespace follows that marker.
bool IsBlankLineText(std::string_view text) {
    if (!text.empty() && (text.front() == '+' || text.front() == '-')) {
        text.remove_prefix(1);
    }
    return text.find_first_not_of(" \t\r\n") == std::string_view::npos;
}

bool IsBlankOnlySequence(const std::vector<TokenId>& tokens, const TokenInterner& interner) {
    return std::all_of(tokens.begin(), tokens.end(),
                        [&](const TokenId token) { return IsBlankLineText(interner.TextFor(token)); });
}

// A change consisting only of blank added/removed lines carries no review
// value, so it must not appear in any rendered output. `ChangeSelector`
// deliberately keeps such changes in its result (detection/selection see the
// full picture); hiding them is a display concern, so it lives here.
ChangeSelection FilterBlankOnlyChanges(const ChangeSelection& selection, const TokenInterner& interner) {
    ChangeSelection filtered;
    for (const auto& change : selection.commonChanges) {
        if (!IsBlankOnlySequence(change.tokens, interner)) {
            filtered.commonChanges.push_back(change);
        }
    }
    for (const auto& unique : selection.uniqueLines) {
        if (!IsBlankLineText(interner.TextFor(unique.line.token))) {
            filtered.uniqueLines.push_back(unique);
        }
    }
    return filtered;
}

// Appends `text` to `out` as a quoted, escaped JSON string.
void AppendJsonString(std::ostream& out, std::string_view text) {
    out << '"';
    for (const unsigned char c : text) {
        switch (c) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (c < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out << buf;
                } else {
                    out << static_cast<char>(c);
                }
        }
    }
    out << '"';
}

// Writes a 2-space-indented JSON array of `count` items: `writeItem(i)`
// renders item `i` (including its own closing brace/indent, if any).
// `itemIndent`/`closeIndent` are the indentation of each item and of the
// array's closing bracket, matching JSON.stringify(x, null, 2) output.
void WriteJsonArray(std::ostream& out, std::size_t count, std::string_view itemIndent,
                    std::string_view closeIndent, const std::function<void(std::size_t)>& writeItem) {
    out << '[';
    for (std::size_t i = 0; i < count; ++i) {
        out << (i == 0 ? "\n" : ",\n") << itemIndent;
        writeItem(i);
    }
    if (count != 0) {
        out << '\n' << closeIndent;
    }
    out << ']';
}

}  // namespace

OutputRenderer::OutputRenderer(const ChangeSelection& selection, const TokenInterner& interner)
    : selection_(selection), interner_(interner) {}

std::optional<OutputFormat> ParseOutputFormat(std::string_view value) {
    std::string lowerValue(value);
    std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    if (lowerValue == "txt") {
        return OutputFormat::Text;
    }
    if (lowerValue == "md") {
        return OutputFormat::Markdown;
    }
    if (lowerValue == "json") {
        return OutputFormat::Json;
    }
    return std::nullopt;
}

void OutputRenderer::Render(std::ostream& out, OutputFormat format) const {
    const ChangeSelection filtered = FilterBlankOnlyChanges(selection_, interner_);
    switch (format) {
        case OutputFormat::Text:
            RenderText(out, filtered);
            return;
        case OutputFormat::Markdown:
            RenderMarkdown(out, filtered);
            return;
        case OutputFormat::Json:
            RenderJson(out, filtered);
            return;
    }
    throw std::invalid_argument("OutputRenderer::Render: unknown OutputFormat");
}

void OutputRenderer::RenderText(std::ostream& out, const ChangeSelection& selection) const {
    const TokenInterner& interner = interner_;
    out << selection.commonChanges.size() << " common change(s), " << selection.uniqueLines.size()
        << " unique line(s).\n";

    for (std::size_t i = 0; i < selection.commonChanges.size(); ++i) {
        const auto& change = selection.commonChanges[i];
        out << "\nCommon change " << (i + 1) << " (" << change.occurrences.size() << " occurrence(s), "
            << change.tokens.size() << " line(s)):\n";
        for (const auto token : change.tokens) {
            out << "  " << interner.TextFor(token) << '\n';
        }
        out << "  Occurrences:\n";
        for (const auto& group : GroupOccurrencesByFile(change.occurrences)) {
            out << "    " << group.filePath << ':';
            WriteCommaSeparated(out, group.startLines);
            out << '\n';
        }
    }

    if (!selection.uniqueLines.empty()) {
        out << "\nUnique changes:";
        for (const auto& group : GroupByFile(selection.uniqueLines)) {
            out << "\n" << group.filePath << ":\n";
            const auto hunks = BuildHunks(group.lines, interner);
            for (std::size_t h = 0; h < hunks.size(); ++h) {
                if (h > 0) {
                    out << '\n';
                }
                out << "  " << FormatHunkHeader(hunks[h]) << '\n';
                for (const auto& text : hunks[h].lineTexts) {
                    out << "  " << text << '\n';
                }
            }
        }
    }
}

void OutputRenderer::RenderMarkdown(std::ostream& out, const ChangeSelection& selection) const {
    const TokenInterner& interner = interner_;
    out << "**" << selection.commonChanges.size() << " common change(s), " << selection.uniqueLines.size()
        << " unique line(s).**\n";

    for (std::size_t i = 0; i < selection.commonChanges.size(); ++i) {
        const auto& change = selection.commonChanges[i];
        out << "\n### Common change " << (i + 1) << " (" << change.occurrences.size() << " occurrence(s), "
            << change.tokens.size() << " line(s))\n\n```diff\n";
        for (const auto token : change.tokens) {
            out << interner.TextFor(token) << '\n';
        }
        out << "```\n\n**Occurrences:**\n\n";
        for (const auto& group : GroupOccurrencesByFile(change.occurrences)) {
            out << "- `" << group.filePath << ':';
            WriteCommaSeparated(out, group.startLines);
            out << "`\n";
        }
    }

    if (!selection.uniqueLines.empty()) {
        out << "\n### Unique changes\n";
        for (const auto& group : GroupByFile(selection.uniqueLines)) {
            out << "\n**" << group.filePath << "**\n\n```diff\n";
            for (const auto& hunk : BuildHunks(group.lines, interner)) {
                out << FormatHunkHeader(hunk) << '\n';
                for (const auto& text : hunk.lineTexts) {
                    out << text << '\n';
                }
            }
            out << "```\n";
        }
    }
}

// Pretty-printed JSON shape (2-space indent, matching e.g. JSON.stringify(x, null, 2)):
// {
//   "commonChanges": [{"length":N,"occurrenceCount":N,"lines":[...],
//     "occurrences":[{"filePath":...,"startLine":N}, ...]}, ...],
//   "uniqueChanges": [{"filePath":...,
//     "hunks":[{"oldStart":N,"oldCount":N,"newStart":N,"newCount":N,"lines":[...]}, ...]}, ...]
// }
void OutputRenderer::RenderJson(std::ostream& out, const ChangeSelection& selection) const {
    const TokenInterner& interner = interner_;
    out << "{\n";

    out << "  \"commonChanges\": ";
    WriteJsonArray(out, selection.commonChanges.size(), "    ", "  ", [&](std::size_t i) {
        const auto& change = selection.commonChanges[i];
        out << "{\n";
        out << "      \"length\": " << change.tokens.size() << ",\n";
        out << "      \"occurrenceCount\": " << change.occurrences.size() << ",\n";
        out << "      \"lines\": ";
        WriteJsonArray(out, change.tokens.size(), "        ", "      ", [&](std::size_t k) {
            AppendJsonString(out, interner.TextFor(change.tokens[k]));
        });
        out << ",\n";
        out << "      \"occurrences\": ";
        WriteJsonArray(out, change.occurrences.size(), "        ", "      ", [&](std::size_t k) {
            const auto& occurrence = change.occurrences[k];
            out << "{\n";
            out << "          \"filePath\": ";
            AppendJsonString(out, occurrence.filePath);
            out << ",\n          \"startLine\": " << occurrence.startLine << "\n        }";
        });
        out << "\n    }";
    });
    out << ",\n";

    const auto groups = GroupByFile(selection.uniqueLines);
    out << "  \"uniqueChanges\": ";
    WriteJsonArray(out, groups.size(), "    ", "  ", [&](std::size_t i) {
        const auto& group = groups[i];
        out << "{\n";
        out << "      \"filePath\": ";
        AppendJsonString(out, group.filePath);
        out << ",\n      \"hunks\": ";
        const auto hunks = BuildHunks(group.lines, interner);
        WriteJsonArray(out, hunks.size(), "        ", "      ", [&](std::size_t h) {
            const auto& hunk = hunks[h];
            out << "{\n";
            out << "          \"oldStart\": " << hunk.oldStart << ",\n";
            out << "          \"oldCount\": " << hunk.oldCount << ",\n";
            out << "          \"newStart\": " << hunk.newStart << ",\n";
            out << "          \"newCount\": " << hunk.newCount << ",\n";
            out << "          \"lines\": ";
            WriteJsonArray(out, hunk.lineTexts.size(), "            ", "          ", [&](std::size_t k) {
                AppendJsonString(out, hunk.lineTexts[k]);
            });
            out << "\n        }";
        });
        out << "\n    }";
    });
    out << "\n";

    out << "}\n";
}

}  // namespace git_diff_pivot
