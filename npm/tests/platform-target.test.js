'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const { getPlatformTarget } = require('../root/lib/platform-target');

test('maps win32/x64 to the Windows platform package and .exe binary', () => {
  assert.deepEqual(getPlatformTarget('win32', 'x64'), {
    packageName: '@git-diff-pivot/win32-x64',
    binaryName: 'git-diff-pivot.exe',
  });
});

test('maps linux/x64 to the Linux platform package and extension-less binary', () => {
  assert.deepEqual(getPlatformTarget('linux', 'x64'), {
    packageName: '@git-diff-pivot/linux-x64',
    binaryName: 'git-diff-pivot',
  });
});

test('maps darwin/arm64 to the macOS platform package', () => {
  assert.deepEqual(getPlatformTarget('darwin', 'arm64'), {
    packageName: '@git-diff-pivot/darwin-arm64',
    binaryName: 'git-diff-pivot',
  });
});

test('throws a clear error for platform/arch combinations with no published package', () => {
  assert.throws(
    () => getPlatformTarget('linux', 'arm64'),
    /does not publish a native binary for platform "linux-arm64"/
  );
});
