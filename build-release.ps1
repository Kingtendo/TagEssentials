[CmdletBinding()]
param(
    [string]$ExecutableName = "TagEssentials.exe",
    [string]$NodeVersion = "v22.23.2"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$projectDirectory = $PSScriptRoot
$buildDirectory = Join-Path $projectDirectory "build"
$releaseDirectory = Join-Path $projectDirectory "x64\Release"
$launcherProject = Join-Path $projectDirectory "TagEssentials.vcxproj"
$packerProject = Join-Path $projectDirectory "RuntimeBundlePacker.vcxproj"
$packerExecutable = Join-Path $releaseDirectory "RuntimeBundlePacker.exe"
$standaloneBuildDirectory = Join-Path $buildDirectory "standalone-runtime"
$runtimeDirectory = Join-Path $standaloneBuildDirectory "runtime"
$nodeCacheDirectory = Join-Path $standaloneBuildDirectory "node-cache"
$nodeArchivePath = Join-Path $nodeCacheDirectory "node-$NodeVersion-win-x64.zip"
$nodePackageDirectory = Join-Path $nodeCacheDirectory "node-$NodeVersion-win-x64"
$nodeExecutable = Join-Path $nodePackageDirectory "node.exe"
$npmExecutable = Join-Path $nodePackageDirectory "npm.cmd"
$bundlePath = Join-Path $buildDirectory "mutedVoiceRuntime.bundle"
$versionPath = Join-Path $buildDirectory "mutedVoiceRuntime.version"
$embeddedResourcePath = Join-Path $buildDirectory "embedded_runtime.rc"
$publicHelpersSource = Join-Path $projectDirectory "public_helpers_server.txt"
$publicHelpersBundlePath = Join-Path $buildDirectory "public_helpers_server.txt"
$builtExecutable = Join-Path $releaseDirectory "TagEssentials.exe"
$standaloneExecutable = Join-Path $buildDirectory $ExecutableName

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE."
    }
}

function Remove-BuildDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$Target,
        [Parameter(Mandatory = $true)][string]$AllowedParent
    )

    if (-not (Test-Path -LiteralPath $Target)) { return }
    $resolvedTarget = [IO.Path]::GetFullPath($Target)
    $resolvedParent = [IO.Path]::GetFullPath($AllowedParent).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if (-not $resolvedTarget.StartsWith($resolvedParent + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove a build directory outside $resolvedParent."
    }
    Remove-Item -LiteralPath $Target -Recurse -Force
}

function Resolve-MSBuild {
    $command = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $programFilesX86 = ${env:ProgramFiles(x86)}
    $vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw "MSBuild was not found. Install Visual Studio 2022 with Desktop development with C++."
    }

    $installationPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
    if (-not $installationPath) {
        throw "A Visual Studio installation containing MSBuild was not found."
    }
    return Join-Path $installationPath "MSBuild\Current\Bin\MSBuild.exe"
}

function Get-NodeMajorVersion {
    param([Parameter(Mandatory = $true)][string]$NodePath)

    $versionText = & $NodePath --version
    if ($LASTEXITCODE -ne 0 -or $versionText -notmatch '^v(\d+)') { return 0 }
    return [int]$Matches[1]
}

function Ensure-Node22Toolchain {
    $installedNode = Get-Command node.exe -ErrorAction SilentlyContinue
    $installedNpm = Get-Command npm.cmd -ErrorAction SilentlyContinue
    if ($installedNode -and $installedNpm -and (Get-NodeMajorVersion $installedNode.Source) -ge 22) {
        return @($installedNode.Source, $installedNpm.Source, (Split-Path -Parent $installedNode.Source))
    }

    New-Item -ItemType Directory -Path $nodeCacheDirectory -Force | Out-Null
    $nodeArchiveName = Split-Path -Leaf $nodeArchivePath
    $nodeUrl = "https://nodejs.org/dist/$NodeVersion/$nodeArchiveName"
    $checksumUrl = "https://nodejs.org/dist/$NodeVersion/SHASUMS256.txt"

    if (-not (Test-Path -LiteralPath (Join-Path $nodePackageDirectory "node.exe") -PathType Leaf)) {
        if (-not (Test-Path -LiteralPath $nodeArchivePath -PathType Leaf)) {
            Write-Host "Downloading Node.js $NodeVersion for the embedded runtime..."
            Invoke-WebRequest -Uri $nodeUrl -OutFile $nodeArchivePath -UseBasicParsing
        }

        $checksumLines = (Invoke-WebRequest -Uri $checksumUrl -UseBasicParsing).Content -split "`r?`n"
        $checksumLine = $checksumLines | Where-Object { $_ -match ("\s" + [regex]::Escape($nodeArchiveName) + "$") } | Select-Object -First 1
        if (-not $checksumLine -or $checksumLine -notmatch '^([0-9a-fA-F]{64})\s+') {
            throw "The Node.js checksum for $nodeArchiveName was not found."
        }
        $expectedChecksum = $Matches[1].ToLowerInvariant()
        $actualChecksum = (Get-FileHash -LiteralPath $nodeArchivePath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actualChecksum -ne $expectedChecksum) {
            throw "The downloaded Node.js archive failed its SHA256 checksum."
        }

        Remove-BuildDirectory $nodePackageDirectory $nodeCacheDirectory
        Expand-Archive -LiteralPath $nodeArchivePath -DestinationPath $nodeCacheDirectory -Force
    }

    if (-not (Test-Path -LiteralPath $nodeExecutable -PathType Leaf) -or
        -not (Test-Path -LiteralPath $npmExecutable -PathType Leaf)) {
        throw "The Node.js $NodeVersion archive did not contain node.exe and npm.cmd."
    }
    return @($nodeExecutable, $npmExecutable, $nodePackageDirectory)
}

if ([IO.Path]::GetFileName($ExecutableName) -ne $ExecutableName -or
    [IO.Path]::GetExtension($ExecutableName).ToLowerInvariant() -ne ".exe") {
    throw "ExecutableName must be a filename ending in .exe."
}

$msbuildPath = Resolve-MSBuild
New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $standaloneBuildDirectory -Force | Out-Null
Remove-BuildDirectory $runtimeDirectory $standaloneBuildDirectory
New-Item -ItemType Directory -Path $runtimeDirectory -Force | Out-Null

Invoke-Checked $msbuildPath @(
    $packerProject,
    "/t:Rebuild",
    "/p:Configuration=Release",
    "/p:Platform=x64",
    "/m"
)
if (-not (Test-Path -LiteralPath $packerExecutable -PathType Leaf)) {
    throw "The runtime bundle packer was not built: $packerExecutable"
}

$nodeTools = Ensure-Node22Toolchain
$nodeExecutablePath = $nodeTools[0]
$npmExecutablePath = $nodeTools[1]
$nodePackageDirectoryPath = $nodeTools[2]
Copy-Item -LiteralPath (Join-Path $projectDirectory "mutedVoiceBot.js") -Destination $runtimeDirectory -Force
Copy-Item -LiteralPath (Join-Path $projectDirectory "package.json") -Destination $runtimeDirectory -Force
Copy-Item -LiteralPath (Join-Path $projectDirectory "package-lock.json") -Destination $runtimeDirectory -Force
Copy-Item -LiteralPath (Join-Path $projectDirectory "LICENSE") -Destination (Join-Path $runtimeDirectory "PROJECT_LICENSE.txt") -Force
Copy-Item -LiteralPath (Join-Path $projectDirectory "THIRD_PARTY_NOTICES.md") -Destination $runtimeDirectory -Force

Push-Location $runtimeDirectory
try {
    & $npmExecutablePath ci --omit=dev --ignore-scripts --no-audit --no-fund
    if ($LASTEXITCODE -ne 0) {
        throw "npm ci failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

Copy-Item -LiteralPath $nodeExecutablePath -Destination (Join-Path $runtimeDirectory "node.exe") -Force
Copy-Item -LiteralPath (Join-Path $nodePackageDirectoryPath "LICENSE") -Destination (Join-Path $runtimeDirectory "NODE_LICENSE.txt") -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $runtimeDirectory "node_modules\.bin") -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $runtimeDirectory "node_modules\.package-lock.json") -Force -ErrorAction SilentlyContinue

Invoke-Checked $packerExecutable @(
    "--input",
    $runtimeDirectory,
    "--output",
    $bundlePath
)
if (-not (Test-Path -LiteralPath $bundlePath -PathType Leaf)) {
    throw "The muted utilities runtime bundle was not created: $bundlePath"
}
Invoke-Checked $packerExecutable @(
    "--verify",
    $bundlePath
)

$bundleVersion = (Get-FileHash -LiteralPath $bundlePath -Algorithm SHA256).Hash.ToLowerInvariant()
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($versionPath, "$bundleVersion`n", $utf8NoBom)

$resourceLines = @(
    '#include "../resource.h"',
    '',
    'IDR_TAGESSENTIALS_RUNTIME RCDATA EMBEDDED_RUNTIME_BUNDLE',
    'IDR_TAGESSENTIALS_RUNTIME_VERSION RCDATA EMBEDDED_RUNTIME_VERSION'
)

if (Test-Path -LiteralPath $publicHelpersSource -PathType Leaf) {
    $publicHelpersLines = @(Get-Content -LiteralPath $publicHelpersSource | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    if ($publicHelpersLines.Count -lt 2 -or
        $publicHelpersLines[0] -notmatch '^https://[^/\s]+(?:/.*)?$' -or
        $publicHelpersLines[0] -match 'example\.com' -or
        $publicHelpersLines[1].Length -lt 24 -or
        $publicHelpersLines[1] -match '^replace-') {
        throw "public_helpers_server.txt exists but is invalid. Fix it or remove it to build without Public Helpers."
    }
    Copy-Item -LiteralPath $publicHelpersSource -Destination $publicHelpersBundlePath -Force
    $resourceLines += 'IDR_TAGESSENTIALS_PUBLIC_HELPERS RCDATA EMBEDDED_PUBLIC_HELPERS'
} else {
    Remove-Item -LiteralPath $publicHelpersBundlePath -Force -ErrorAction SilentlyContinue
}

[IO.File]::WriteAllText($embeddedResourcePath, (($resourceLines -join "`r`n") + "`r`n"), $utf8NoBom)

Invoke-Checked $msbuildPath @(
    $launcherProject,
    "/t:Rebuild",
    "/p:Configuration=Release",
    "/p:Platform=x64",
    "/m"
)
if (-not (Test-Path -LiteralPath $builtExecutable -PathType Leaf)) {
    throw "The release executable was not built: $builtExecutable"
}

Copy-Item -LiteralPath $builtExecutable -Destination $standaloneExecutable -Force
if (-not (Test-Path -LiteralPath $standaloneExecutable -PathType Leaf)) {
    throw "The standalone executable was not created: $standaloneExecutable"
}

Write-Host "Standalone TagEssentials executable created:"
Write-Host "  $standaloneExecutable"
Write-Host "  Size: $([Math]::Round((Get-Item -LiteralPath $standaloneExecutable).Length / 1MB, 1)) MB"
Write-Host "The generated EXE is ignored by Git and is not included in the source repository."
