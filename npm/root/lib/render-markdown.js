'use strict';

// Mirrors OutputRenderer::RenderMarkdown (src/git_diff_pivot/output_renderer.cpp):
// same structure as jsonToText, rendered as Markdown with fenced ```diff blocks
// Operates on the parsed `--output-type json` result shape.

// Formats a hunk's line range as a git-style header, omitting the ",count"
// part when it's 1 (as git itself does).
function formatHunkHeader(hunk) {
  const oldPart = hunk.oldCount === 1 ? `${hunk.oldStart}` : `${hunk.oldStart},${hunk.oldCount}`;
  const newPart = hunk.newCount === 1 ? `${hunk.newStart}` : `${hunk.newStart},${hunk.newCount}`;
  return `@@ -${oldPart} +${newPart} @@`;
}

function countUniqueLines(uniqueChanges) {
  let count = 0;
  for (const group of uniqueChanges) {
    for (const hunk of group.hunks) {
      count += hunk.lines.length;
    }
  }
  return count;
}

function jsonToMarkdown(result) {
  const { commonChanges, uniqueChanges } = result;
  const parts = [
    `**${commonChanges.length} common change(s), ${countUniqueLines(uniqueChanges)} unique line(s).**\n`,
  ];

  commonChanges.forEach((change, index) => {
    parts.push(`\n### Common change ${index + 1}\n\n\`\`\`diff\n`);
    for (const line of change.lines) {
      parts.push(`${line}\n`);
    }
    parts.push(`\`\`\`\n\n<details>\n<summary>${change.occurrenceCount} occurrences</summary>\n\n`);
    for (const occurrence of change.occurrences) {
      parts.push(`- ${occurrence.filePath}: [${occurrence.startLine.join(', ')}]\n`);
    }
    parts.push('\n</details>\n');
  });

  if (uniqueChanges.length > 0) {
    parts.push('\n### Unique changes\n');
    for (const group of uniqueChanges) {
      parts.push(`\n**${group.filePath}**\n\n\`\`\`diff\n`);
      for (const hunk of group.hunks) {
        parts.push(`${formatHunkHeader(hunk)}\n`);
        for (const line of hunk.lines) {
          parts.push(`${line}\n`);
        }
      }
      parts.push('```\n');
    }
  }

  return parts.join('');
}

module.exports = { jsonToMarkdown };
