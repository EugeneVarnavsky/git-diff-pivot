#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "git_diff_pivot/change_selector.hpp"
#include "git_diff_pivot/token_interner.hpp"

namespace git_diff_pivot {

// Selects which compressed-diff format OutputRenderer produces.
enum class OutputFormat {
    Text,      // Human-readable plain text (the original/default format).
    Markdown,  // Markdown, suitable for a GitHub Actions job summary.
    Json,      // Compact, machine-readable JSON for external tooling.
};

// Parses a `--output-type` CLI value ("txt", "md", "json"); returns
// std::nullopt for anything else. Matching is case-sensitive.
[[nodiscard]] std::optional<OutputFormat> ParseOutputFormat(std::string_view value);

// Renders the compressed diff, i.e. inverts "file -> changes" into
// "unique change -> files/occurrences". Each accepted common
// change is displayed once with every occurrence's file/line, followed by
// every unique line grouped under its original file.
class OutputRenderer {
public:
    [[nodiscard]] std::string Render(const ChangeSelection& selection, const TokenInterner& interner,
                                      OutputFormat format = OutputFormat::Text) const;

private:
    [[nodiscard]] std::string RenderText(const ChangeSelection& selection, const TokenInterner& interner) const;
    [[nodiscard]] std::string RenderMarkdown(const ChangeSelection& selection, const TokenInterner& interner) const;
    [[nodiscard]] std::string RenderJson(const ChangeSelection& selection, const TokenInterner& interner) const;
};

}  // namespace git_diff_pivot
