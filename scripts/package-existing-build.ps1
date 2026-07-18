[CmdletBinding()]
param(
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',

    [string] $Version
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$StageRoot = Join-Path $RepoRoot "release/stage/$Configuration"

if (-not $Version) {
    $Version = (Get-Content (Join-Path $RepoRoot 'buildspec.json') -Raw | ConvertFrom-Json).version
}

if (-not (Test-Path -LiteralPath $StageRoot -PathType Container)) {
    throw "Existing build stage was not found: $StageRoot`nRun build-local-windows.bat once before using package-only mode."
}

Write-Host '[INFO] Reusing the existing successful build. No OBS dependencies will be downloaded or rebuilt.'
& (Join-Path $PSScriptRoot 'package-windows.ps1') `
    -StageRoot $StageRoot `
    -Version $Version `
    -Configuration $Configuration
