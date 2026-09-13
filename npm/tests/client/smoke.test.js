'use strict';

// Real-environment smoke test: unlike npm/tests/*.test.js (which require the
// repo's lib/ files directly), this package only has "git-diff-pivot" and
// "@git-diff-pivot/win32-x64" installed as ordinary dependencies (from the
// tarballs in npm/tests/dist), so this exercises the actual published entry
// point, module resolution, and native binary spawn a real consumer would use.
const test = require('node:test');
const assert = require('node:assert/strict');
const { diffToText, diffToJson } = require('git-diff-pivot');

const diffText =
  'diff --git a/src/alpha.cpp b/src/alpha.cpp\n' +
  '--- a/src/alpha.cpp\n' +
  '+++ b/src/alpha.cpp\n' +
  '@@ -0,0 +1,2 @@\n' +
  '+int result = 0;\n' +
  '+// alpha-specific line\n' +
  'diff --git a/src/beta.cpp b/src/beta.cpp\n' +
  '--- a/src/beta.cpp\n' +
  '+++ b/src/beta.cpp\n' +
  '@@ -0,0 +1,2 @@\n' +
  '+int result = 0;\n' +
  '+// beta-specific line\n';

test('diffToText runs the real native binary and renders the compressed diff', async () => {
  const rendered = await diffToText(diffText);
  assert.match(rendered, /Common change 1 \(2 occurrence\(s\), 1 line\(s\)\)/);
  assert.match(rendered, /\+int result = 0;/);
  assert.match(rendered, /\+\/\/ alpha-specific line/);
  assert.match(rendered, /\+\/\/ beta-specific line/);
});

test('diffToJson runs the real native binary and returns structured output', async () => {
  const result = await diffToJson(diffText);
  assert.equal(result.commonChanges.length, 1);
  assert.equal(result.commonChanges[0].occurrenceCount, 2);
  assert.equal(result.uniqueChanges.length, 2);
});
