'use strict';

// Mirrors OutputRenderer::RenderText (src/git_diff_pivot/output_renderer.cpp):
// each common change once with every occurrence's file/lines grouped, followed
// by unique lines grouped by file, each hunk starting with a git-style
// "@@ -oldStart,oldCount +newStart,newCount @@" header.
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

function jsonToText(result) {
  const { commonChanges, uniqueChanges } = result;
  const parts = [`${commonChanges.length} common change(s), ${countUniqueLines(uniqueChanges)} unique line(s).\n`];

  commonChanges.forEach((change, index) => {
    parts.push(`\nCommon change ${index + 1}:\n`);
    for (const line of change.lines) {
      parts.push(`  ${line}\n`);
    }
    parts.push(`\n  ${change.occurrenceCount} occurrences:\n`);
    for (const occurrence of change.occurrences) {
      parts.push(`    ${occurrence.filePath}: [${occurrence.startLine.join(', ')}]\n`);
    }
  });

  if (uniqueChanges.length > 0) {
    parts.push('\nUnique changes:\n');
    for (const group of uniqueChanges) {
      parts.push(`\n${group.filePath}:\n`);
      group.hunks.forEach((hunk, hunkIndex) => {
        if (hunkIndex > 0) {
          parts.push('\n');
        }
        parts.push(`  ${formatHunkHeader(hunk)}\n`);
        for (const line of hunk.lines) {
          parts.push(`  ${line}\n`);
        }
      });
    }
  }

  return parts.join('');
}

module.exports = { jsonToText };
