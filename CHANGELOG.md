# Changelog

## 0.2.2

- Fixed a bug where repeated changed lines could be reported as unique when they overlapped a more frequent common change.

## 0.2.1

- Text and Markdown output: common changes now list their occurrences grouped by file (e.g. `FileA: [1, 10, 20]`) instead of one line per occurrence.
- JSON output: `commonChanges[].occurrences[].startLine` is now an array of line numbers grouped per file, instead of one `occurrences` entry per occurrence.
- CLI: added `--txt`, `--md`, `--json` shorthand flags for `--output-type`.

## 0.2.0

- Input reading and result output switched to streaming to reduce memory usage.

## 0.1.0

- Initial release.
