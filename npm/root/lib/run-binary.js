'use strict';

const { spawn } = require('child_process');
const { resolveBinaryPath } = require('./resolve-binary-path');

// Builds the CLI args for `outputType`, forwarding `options.minLength`/
// `options.minOccurrences` as --min-length/--min-occurrences when given.
function buildArgs(outputType, options) {
  const args = ['--output-type', outputType];
  if (options.minLength !== undefined) {
    args.push('--min-length', String(options.minLength));
  }
  if (options.minOccurrences !== undefined) {
    args.push('--min-occurrences', String(options.minOccurrences));
  }
  return args;
}

// Runs the native git-diff-pivot binary against `diffText`, requesting
// `outputType` ("json"/"txt"/"md"), and resolves with its stdout. Streams
// output into chunks and joins them once at the end, so output size is
// bounded only by available memory, not by a fixed buffer limit.
function runGitDiffPivot(diffText, outputType, options = {}) {
  return new Promise((resolve, reject) => {
    const binaryPath = resolveBinaryPath();
    const child = spawn(binaryPath, buildArgs(outputType, options));

    const stdoutChunks = [];
    const stderrChunks = [];
    child.stdout.setEncoding('utf8');
    child.stderr.setEncoding('utf8');
    child.stdout.on('data', (chunk) => stdoutChunks.push(chunk));
    child.stderr.on('data', (chunk) => stderrChunks.push(chunk));

    child.on('error', (error) => {
      reject(new Error(`git-diff-pivot: failed to launch "${binaryPath}": ${error.message}`));
    });

    child.on('close', (code) => {
      if (code !== 0) {
        reject(new Error(`git-diff-pivot exited with code ${code}: ${stderrChunks.join('') || '(no error output)'}`));
        return;
      }
      resolve(stdoutChunks.join(''));
    });

    child.stdin.end(diffText);
  });
}

module.exports = { runGitDiffPivot };
