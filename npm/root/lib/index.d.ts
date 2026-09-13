// Type declarations for the git-diff-pivot library API
// Mirrors the JSON shape produced by `git-diff-pivot --output-type json`

export interface GitDiffPivotOccurrence {
  filePath: string;
  startLine: number;
}

export interface GitDiffPivotCommonChange {
  length: number;
  occurrenceCount: number;
  lines: string[];
  occurrences: GitDiffPivotOccurrence[];
}

export interface GitDiffPivotUniqueChangeHunk {
  oldStart: number;
  oldCount: number;
  newStart: number;
  newCount: number;
  lines: string[];
}

export interface GitDiffPivotUniqueChange {
  filePath: string;
  hunks: GitDiffPivotUniqueChangeHunk[];
}

export interface GitDiffPivotResult {
  commonChanges: GitDiffPivotCommonChange[];
  uniqueChanges: GitDiffPivotUniqueChange[];
}

export interface GitDiffPivotOptions {
  /** Minimum common-change length in lines (CLI --min-length, default 1). */
  minLength?: number;
  /** Minimum occurrences for a common change (CLI --min-occurrences, default 2). */
  minOccurrences?: number;
}

/** Converts a unified Git diff into the parsed `--output-type json` result. */
export function diffToJson(diffText: string, options?: GitDiffPivotOptions): Promise<GitDiffPivotResult>;

/** Renders a `GitDiffPivotResult` the same way `--output-type txt` does. */
export function jsonToText(result: GitDiffPivotResult): string;

/** Renders a `GitDiffPivotResult` the same way `--output-type md` does. */
export function jsonToMarkdown(result: GitDiffPivotResult): string;

/** Converts a unified Git diff directly into the CLI's `txt` rendering. */
export function diffToText(diffText: string, options?: GitDiffPivotOptions): Promise<string>;

/** Converts a unified Git diff directly into the CLI's `md` rendering. */
export function diffToMarkdown(diffText: string, options?: GitDiffPivotOptions): Promise<string>;
