# git-diff-pivot

`git-diff-pivot` is a command-line tool that analyzes a multi-file Git diff and finds identical sequences of changed lines that occur in multiple places, so a reviewer can see a repeated change once instead of once per occurrence.

## What it does

`git-diff-pivot` parses a unified Git diff into files, hunks, and changed lines, detects repeated changed-line sequences, greedily selects a conflict-free set of them, and prints each common change once with every file/line occurrence, followed by any remaining file-specific changes.

## Installation

`git-diff-pivot` is built from source with CMake. Requires CMake 3.25+ and a C++20 compiler (MSVC on Windows, or GCC/Clang on Linux/macOS).

```powershell
cmake --preset windows
cmake --build --preset windows-release
```

```bash
cmake --preset linux      # or: cmake --preset macos
cmake --build --preset linux-release
```

The built executable is written to `build/<preset-name>/src/Release/git-diff-pivot` (`git-diff-pivot.exe` on Windows).

## Third-party dependencies

- [`libsais`](https://github.com/IlyaGrebnov/libsais) is used by the application for suffix-array construction. It is licensed under the [Apache License, Version 2.0](https://www.apache.org/licenses/LICENSE-2.0) (`Apache-2.0`).
- [`Catch2`](https://github.com/catchorg/Catch2) is used for testing. It is licensed under the [Boost Software License, Version 1.0](https://www.boost.org/LICENSE_1_0.txt) (`BSL-1.0`).

## Usage

```powershell
git-diff-pivot path\to\diff.txt
git diff | git-diff-pivot
```

`git-diff-pivot` reads a unified Git diff from the given file, or from stdin if no file is given, and prints a compressed, review-oriented view: each repeated changed-line sequence is shown once with every file/line occurrence, followed by any remaining file-specific changes.

### Options

```text
git-diff-pivot [options] [file]
  --min-length <N>       Minimum common-change length in lines (default: 1)
  --min-occurrences <N>  Minimum occurrences for a common change (default: 2)
  --output <path>        Write output to this file instead of stdout
  --output-type <type>   Output format: txt, md, or json (default: txt, or
                         inferred from --output's file extension)
  -h, --help             Show this help message
  -v, --version          Show the product version
```

`--output-type md` produces a Markdown rendering suitable for pasting into a GitHub Actions job summary; `--output-type json` produces a compact, machine-readable rendering for external tooling. `--output` and `--output-type` can be given independently: if `--output` is given without `--output-type`, the format is inferred from the output file's extension (an unrecognized extension is an error); if `--output-type` is given without `--output`, it only controls the stdout format; an explicit `--output-type` always wins over the inferred extension. Option values can be given as a separate argument (`--output-type md`) or joined with `=` (`--output-type=md`).

## Library usage (Node.js/TypeScript)

The `git-diff-pivot` npm package also works as a library for projects that want to depend on it directly instead of shelling out to the CLI themselves. It ships hand-written TypeScript type declarations, so it works from both TypeScript and plain JavaScript:

```ts
import { diffToJson, jsonToText, jsonToMarkdown, diffToText, diffToMarkdown } from 'git-diff-pivot';

const diffText = /* a unified Git diff, e.g. from `git diff` */ '';

const result = await diffToJson(diffText);   // same shape as --output-type json
const text = jsonToText(result);             // same rendering as --output-type txt
const markdown = jsonToMarkdown(result);     // same rendering as --output-type md

// Or skip the JSON step entirely:
await diffToText(diffText);
await diffToMarkdown(diffText);

// diffToJson/diffToText/diffToMarkdown take an optional options object
// mapping to --min-length/--min-occurrences:
await diffToJson(diffText, { minLength: 2, minOccurrences: 3 });
```

`diffToJson`/`diffToText`/`diffToMarkdown` run the native binary under the hood (like the CLI) and return a `Promise`; `jsonToText`/`jsonToMarkdown` are synchronous, pure functions that only need an already-parsed `GitDiffPivotResult` (from `diffToJson`, or from your own `--output-type json` output) and do not shell out again.
