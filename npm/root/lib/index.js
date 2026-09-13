'use strict';

const { runGitDiffPivot } = require('./run-binary');
const { jsonToText } = require('./render-text');
const { jsonToMarkdown } = require('./render-markdown');

// Converts a unified Git diff into the same structured result produced by `git-diff-pivot --output-type json`.
// `options.minLength`/`options.minOccurrences` map to the CLI's --min-length/--min-occurrences.
async function diffToJson(diffText, options) {
  return JSON.parse(await runGitDiffPivot(diffText, 'json', options));
}

// Converts a unified Git diff directly into the CLI's txt/md renderings,
// equivalent to `git-diff-pivot --output-type txt`/`--output-type md`.
function diffToText(diffText, options) {
  return runGitDiffPivot(diffText, 'txt', options);
}

function diffToMarkdown(diffText, options) {
  return runGitDiffPivot(diffText, 'md', options);
}

module.exports = {
  diffToJson,
  jsonToText,
  jsonToMarkdown,
  diffToText,
  diffToMarkdown,
};
