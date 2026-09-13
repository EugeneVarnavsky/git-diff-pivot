#!/usr/bin/env node
'use strict';

const { spawn } = require('child_process');
const { resolveBinaryPath } = require('../lib/resolve-binary-path');

function main() {
  const binaryPath = resolveBinaryPath();
  const child = spawn(binaryPath, process.argv.slice(2), { stdio: 'inherit' });

  child.on('error', (error) => {
    console.error(`git-diff-pivot: failed to launch "${binaryPath}": ${error.message}`);
    process.exitCode = 1;
  });

  child.on('close', (code, signal) => {
    process.exitCode = signal ? 1 : code ?? 1;
  });
}

try {
  main();
} catch (error) {
  console.error(`git-diff-pivot: ${error.message}`);
  process.exitCode = 1;
}
