# git-diff-pivot

`git-diff-pivot` finds identical sequences of changed lines that repeat across a multi-file Git diff (e.g. the same boilerplate added to several files) and renders a compact, review-oriented view: each repeated change is shown once with every file/line occurrence, instead of once per occurrence. This package ships the native `git-diff-pivot` CLI plus a Node.js/TypeScript library for using the same functionality programmatically.

The `md` output format is purpose-built for reviewing large pull requests: it collapses the same repeated block (a bumped dependency version, a regenerated hash, a re-sorted block) that would otherwise appear dozens or hundreds of times across a diff into a single entry, which is especially useful for reviewing changes to npm lock files (`package-lock.json`, `npm-shrinkwrap.json`) and similar large, repetitive generated files.

## Install

```bash
npm install -g git-diff-pivot   # CLI, globally
npm install git-diff-pivot      # as a project dependency (CLI + library)
npx git-diff-pivot              # run without installing
```

## CLI usage

```bash
git diff | git-diff-pivot
git-diff-pivot path/to/diff.txt
```

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

### Example: a repeated change compressed into Markdown

Given this diff, where two files both add the same two-line logging setup plus one file-specific line:

```diff
diff --git a/src/foo.py b/src/foo.py
--- a/src/foo.py
+++ b/src/foo.py
@@ -1,0 +1,3 @@
+import logging
+logger = logging.getLogger(__name__)
+def foo():
diff --git a/src/bar.py b/src/bar.py
--- a/src/bar.py
+++ b/src/bar.py
@@ -1,0 +1,3 @@
+import logging
+logger = logging.getLogger(__name__)
+def bar():
```

`git-diff-pivot --output-type md` renders the repeated two-line change once, with both occurrences listed, followed by each file's remaining unique line:

````markdown
## Compressed diff summary

**1 common change(s), 2 unique line(s).**

### Common change 1 (2 occurrence(s), 2 line(s))

```diff
+import logging
+logger = logging.getLogger(__name__)
```

**Occurrences:**

- `src/foo.py:1`
- `src/bar.py:1`

### Unique changes

**src/foo.py**

```diff
+def foo():
```

**src/bar.py**

```diff
+def bar():
```
````

## Library usage (Node.js/TypeScript)

The package's `main`/`types` entry point exposes the same functionality as a library, for projects that want to depend on `git-diff-pivot` directly instead of shelling out to its CLI themselves. It ships hand-written TypeScript type declarations, so it works from both TypeScript and plain JavaScript:

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

For the JSON result's exact shape (`GitDiffPivotResult`, `GitDiffPivotCommonChange`, `GitDiffPivotUniqueChange`, ...), see the exported types in `lib/index.d.ts`.

