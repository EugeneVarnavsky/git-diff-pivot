'use strict';

// Maps process.platform + process.arch to the optional
// platform package and the native binary file names

const PLATFORM_PACKAGES = {
  'win32-x64': '@git-diff-pivot/win32-x64',
  'linux-x64': '@git-diff-pivot/linux-x64',
  'darwin-arm64': '@git-diff-pivot/darwin-arm64',
};

const BINARY_NAMES = {
  win32: 'git-diff-pivot.exe',
  linux: 'git-diff-pivot',
  darwin: 'git-diff-pivot',
};

function getPlatformTarget(platform, arch) {
  const key = `${platform}-${arch}`;
  const packageName = PLATFORM_PACKAGES[key];
  const binaryName = BINARY_NAMES[platform];

  if (!packageName || !binaryName) {
    throw new Error(`git-diff-pivot does not publish a native binary for platform "${key}".`);
  }

  return { packageName, binaryName };
}

module.exports = { getPlatformTarget };
