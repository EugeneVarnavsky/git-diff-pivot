#!/usr/bin/env pwsh
# Builds the native Windows Release binary, stages it into the win32-x64
# platform package, packs the root and platform packages into npm/tests/dist
# (simulating what a real `npm install` would fetch from the registry), then
# installs and runs the npm/tests/client "consumer" package against those
# tarballs to exercise the public API through the real native binary.
#
# Usage: npm/test-e2e-windows.ps1

$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$platformDir = Join-Path $repoRoot 'npm/platforms/win32-x64'
$distDir = Join-Path $repoRoot 'npm/tests/dist'
$clientDir = Join-Path $repoRoot 'npm/tests/client'

Push-Location $repoRoot
try {
    Write-Host '==> Building native Windows Release binary' -ForegroundColor Cyan
    cmake --preset windows
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    cmake --build --preset windows-release
    if ($LASTEXITCODE -ne 0) { throw 'CMake Release build failed.' }

    $binary = Get-ChildItem -Path (Join-Path $repoRoot 'build/windows') -Recurse -Filter 'git-diff-pivot.exe' |
        Where-Object { $_.FullName -match '\\Release\\' } |
        Select-Object -First 1
    if (-not $binary) {
        throw 'Could not find a Release build of git-diff-pivot.exe under build/windows.'
    }

    Write-Host "==> Staging $($binary.FullName) into $platformDir" -ForegroundColor Cyan
    Copy-Item $binary.FullName -Destination (Join-Path $platformDir 'git-diff-pivot.exe') -Force

    Write-Host '==> Packing root and win32-x64 packages into npm/tests/dist' -ForegroundColor Cyan
    Remove-Item $distDir -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Path $distDir -Force | Out-Null

    npm pack (Join-Path $repoRoot 'npm/root') --silent --pack-destination $distDir | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'npm pack failed for npm/root.' }
    npm pack $platformDir --silent --pack-destination $distDir | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'npm pack failed for npm/platforms/win32-x64.' }

    # npm/tests/client/package.json depends on these exact tarball names; if a
    # package.json version changed, update that file's dependency versions too.
    $rootVersion = (Get-Content (Join-Path $repoRoot 'npm/root/package.json') -Raw | ConvertFrom-Json).version
    $platformVersion = (Get-Content (Join-Path $platformDir 'package.json') -Raw | ConvertFrom-Json).version
    $rootTarball = Join-Path $distDir "git-diff-pivot-$rootVersion.tgz"
    $platformTarball = Join-Path $distDir "git-diff-pivot-win32-x64-$platformVersion.tgz"
    if (-not (Test-Path $rootTarball)) {
        throw "Expected tarball $rootTarball not found. Update npm/tests/client/package.json if the root package version changed."
    }
    if (-not (Test-Path $platformTarball)) {
        throw "Expected tarball $platformTarball not found. Update npm/tests/client/package.json if the win32-x64 package version changed."
    }

    Write-Host '==> Installing npm/tests/client against the packed tarballs' -ForegroundColor Cyan
    Remove-Item (Join-Path $clientDir 'node_modules') -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $clientDir 'package-lock.json') -Force -ErrorAction SilentlyContinue

    Push-Location $clientDir
    try {
        npm install
        if ($LASTEXITCODE -ne 0) { throw 'npm install failed for npm/tests/client.' }

        Write-Host '==> Running the client smoke test against the real native binary' -ForegroundColor Cyan
        npm test
        if ($LASTEXITCODE -ne 0) { throw 'npm/tests/client smoke test failed.' }
    }
    finally {
        Pop-Location
    }

    Write-Host '==> Windows end-to-end npm smoke test passed' -ForegroundColor Green
}
finally {
    Pop-Location
}
