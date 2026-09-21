#pragma once

#include <optional>
#include <ostream>
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
    OutputRenderer(const ChangeSelection& selection, const TokenInterner& interner);

    void Render(std::ostream& out, OutputFormat format = OutputFormat::Text) const;

private:
    void RenderText(std::ostream& out, const ChangeSelection& selection) const;
    void RenderMarkdown(std::ostream& out, const ChangeSelection& selection) const;
    void RenderJson(std::ostream& out, const ChangeSelection& selection) const;

    const ChangeSelection& selection_;
    const TokenInterner& interner_;
};

}  // namespace git_diff_pivot
