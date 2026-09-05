[CmdletBinding()]
param(
    [string]$ExecutableName = "TagEssentials.exe"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$projectDirectory = $PSScriptRoot
$launcherProject = Join-Path $projectDirectory "TagEssentials.vcxproj"
$releaseDirectory = Join-Path $projectDirectory "x64\Release"
$packageDirectory = Join-Path $projectDirectory "build\release"
$archivePath = Join-Path $projectDirectory "build\TagEssentials-windows-x64.zip"

$msbuildCommand = Get-Command msbuild.exe -ErrorAction SilentlyContinue
if (-not $msbuildCommand) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw "MSBuild was not found. Install Visual Studio 2022 with Desktop development with C++."
    }

    $installationPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
    if (-not $installationPath) {
        throw "A Visual Studio installation containing MSBuild was not found."
    }

    $msbuildPath = Join-Path $installationPath "MSBuild\Current\Bin\MSBuild.exe"
} else {
    $msbuildPath = $msbuildCommand.Source
}

& $msbuildPath $launcherProject /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m
if ($LASTEXITCODE -ne 0) {
    throw "Release build failed with exit code $LASTEXITCODE."
}

$builtExecutable = Join-Path $releaseDirectory "TagEssentials.exe"
$runtimeScript = Join-Path $releaseDirectory "mutedVoiceBot.js"
$releaseGuide = Join-Path $projectDirectory "RELEASE_README.txt"
$packageManifest = Join-Path $projectDirectory "package.json"
$packageLock = Join-Path $projectDirectory "package-lock.json"
$publicHelpersConfig = Join-Path $projectDirectory "public_helpers_server.txt"

foreach ($requiredFile in @($builtExecutable, $runtimeScript, $releaseGuide, $packageManifest, $packageLock)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Expected output was not found: $requiredFile"
    }
}

if (Test-Path -LiteralPath $packageDirectory) {
    $resolvedBuildRoot = [IO.Path]::GetFullPath((Join-Path $projectDirectory "build"))
    $resolvedPackageDirectory = [IO.Path]::GetFullPath($packageDirectory)
    if (-not $resolvedPackageDirectory.StartsWith($resolvedBuildRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clear a package directory outside the build folder."
    }
    Remove-Item -LiteralPath $packageDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path $packageDirectory -Force | Out-Null

Copy-Item -LiteralPath $builtExecutable -Destination (Join-Path $packageDirectory $ExecutableName) -Force
Copy-Item -LiteralPath $runtimeScript -Destination (Join-Path $packageDirectory "mutedVoiceBot.js") -Force
Copy-Item -LiteralPath $releaseGuide -Destination (Join-Path $packageDirectory "README.txt") -Force
Copy-Item -LiteralPath $packageManifest -Destination (Join-Path $packageDirectory "package.json") -Force
Copy-Item -LiteralPath $packageLock -Destination (Join-Path $packageDirectory "package-lock.json") -Force
Copy-Item -LiteralPath (Join-Path $projectDirectory "LICENSE") -Destination (Join-Path $packageDirectory "LICENSE") -Force
Copy-Item -LiteralPath (Join-Path $projectDirectory "THIRD_PARTY_NOTICES.md") -Destination $packageDirectory -Force
foreach ($component in @("minhook", "mcp")) {
    $noticeDirectory = Join-Path $packageDirectory "third_party\$component"
    New-Item -ItemType Directory -Path $noticeDirectory -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectDirectory "third_party\$component\LICENSE.txt") -Destination $noticeDirectory -Force
}

if (Test-Path -LiteralPath $publicHelpersConfig -PathType Leaf) {
    $publicHelpersLines = @(Get-Content -LiteralPath $publicHelpersConfig | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    if ($publicHelpersLines.Count -lt 2 -or
        $publicHelpersLines[0] -notmatch '^https://[^/\s]+(?:/.*)?$' -or
        $publicHelpersLines[0] -match 'example\.com' -or
        $publicHelpersLines[1].Length -lt 24 -or
        $publicHelpersLines[1] -match '^replace-') {
        throw "public_helpers_server.txt exists but is invalid. Fix it or remove it to package without Public Helpers."
    }
    Copy-Item -LiteralPath $publicHelpersConfig -Destination (Join-Path $packageDirectory "public_helpers_server.txt") -Force
}

Compress-Archive -Path (Join-Path $packageDirectory "*") -DestinationPath $archivePath -CompressionLevel Optimal -Force

Write-Host "TagEssentials package created:"
Write-Host "  $archivePath"
