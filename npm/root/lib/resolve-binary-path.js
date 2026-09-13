'use strict';

const path = require('path');
const { getPlatformTarget } = require('./platform-target');

function resolveBinaryPath() {
  const { packageName, binaryName } = getPlatformTarget(process.platform, process.arch);

  let packageJsonPath;
  try {
    packageJsonPath = require.resolve(`${packageName}/package.json`);
  } catch {
    throw new Error(
      `git-diff-pivot native binary package "${packageName}" is not installed. ` +
        `Reinstall git-diff-pivot, or run "npm install ${packageName}" directly.`
    );
  }

  return path.join(path.dirname(packageJsonPath), binaryName);
}

module.exports = { resolveBinaryPath };
