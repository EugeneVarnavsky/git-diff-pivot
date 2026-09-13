'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const { jsonToMarkdown } = require('../../lib/render-markdown');

test('renders an empty result as zero counts and no sections', () => {
  assert.equal(
    jsonToMarkdown({ commonChanges: [], uniqueChanges: [] }),
    '## Compressed diff summary\n\n**0 common change(s), 0 unique line(s).**\n'
  );
});

test('renders the canonical fixture the same way OutputRenderer::RenderMarkdown does', () => {
  const result = {
    commonChanges: [
      {
        length: 3,
        occurrenceCount: 3,
        lines: ['A', 'B', 'C'],
        occurrences: [
          { filePath: 'FileA', startLine: 1 },
          { filePath: 'FileB', startLine: 1 },
          { filePath: 'FileC', startLine: 2 },
        ],
      },
    ],
    uniqueChanges: [
      { filePath: 'FileA', hunks: [{ oldStart: 0, oldCount: 0, newStart: 4, newCount: 1, lines: ['D'] }] },
      { filePath: 'FileB', hunks: [{ oldStart: 0, oldCount: 0, newStart: 4, newCount: 1, lines: ['E'] }] },
      { filePath: 'FileC', hunks: [{ oldStart: 0, oldCount: 0, newStart: 1, newCount: 1, lines: ['X'] }] },
    ],
  };

  assert.equal(
    jsonToMarkdown(result),
    '## Compressed diff summary\n' +
      '\n' +
      '**1 common change(s), 3 unique line(s).**\n' +
      '\n' +
      '### Common change 1 (3 occurrence(s), 3 line(s))\n' +
      '\n' +
      '```diff\n' +
      'A\n' +
      'B\n' +
      'C\n' +
      '```\n' +
      '\n' +
      '**Occurrences:**\n' +
      '\n' +
      '- `FileA:1`\n' +
      '- `FileB:1`\n' +
      '- `FileC:2`\n' +
      '\n' +
      '### Unique changes\n' +
      '\n' +
      '**FileA**\n' +
      '\n' +
      '```diff\n' +
      '@@ -0,0 +4 @@\n' +
      'D\n' +
      '```\n' +
      '\n' +
      '**FileB**\n' +
      '\n' +
      '```diff\n' +
      '@@ -0,0 +4 @@\n' +
      'E\n' +
      '```\n' +
      '\n' +
      '**FileC**\n' +
      '\n' +
      '```diff\n' +
      '@@ -0,0 +1 @@\n' +
      'X\n' +
      '```\n'
  );
});

test('separates unique lines from different original hunks with a git-style hunk header', () => {
  const result = {
    commonChanges: [],
    uniqueChanges: [
      {
        filePath: 'FileA',
        hunks: [
          { oldStart: 0, oldCount: 0, newStart: 1, newCount: 2, lines: ['A', 'B'] },
          { oldStart: 0, oldCount: 0, newStart: 10, newCount: 1, lines: ['C'] },
        ],
      },
    ],
  };

  assert.equal(
    jsonToMarkdown(result),
    '## Compressed diff summary\n' +
      '\n' +
      '**0 common change(s), 3 unique line(s).**\n' +
      '\n' +
      '### Unique changes\n' +
      '\n' +
      '**FileA**\n' +
      '\n' +
      '```diff\n' +
      '@@ -0,0 +1,2 @@\n' +
      'A\n' +
      'B\n' +
      '@@ -0,0 +10 @@\n' +
      'C\n' +
      '```\n'
  );
});
