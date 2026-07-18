[CmdletBinding()]
param(
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',

    [ValidateSet('Auto', '2026', '2022')]
    [string] $VisualStudio = 'Auto',

    [string] $TemplateRef = 'master',
    [switch] $NoPackage,
    [switch] $Ci,
    [switch] $PackageOnly,
    [switch] $RefreshTemplate,
    [switch] $Reconfigure
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$BuildRoot = Join-Path $RepoRoot '.build'
$TemplateDir = Join-Path $BuildRoot 'obs-plugintemplate'
$StageRoot = Join-Path $RepoRoot "release/stage/$Configuration"
$Version = (Get-Content (Join-Path $RepoRoot 'buildspec.json') -Raw | ConvertFrom-Json).version

function Invoke-Logged {
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [string[]]$Arguments = @(),
        [string]$WorkingDirectory = $RepoRoot
    )
    Write-Host "> $FilePath $($Arguments -join ' ')"
    $process = Start-Process -FilePath $FilePath -ArgumentList $Arguments `
        -WorkingDirectory $WorkingDirectory -Wait -NoNewWindow -PassThru
    if ($process.ExitCode -ne 0) {
        throw "$FilePath failed with exit code $($process.ExitCode)"
    }
}

New-Item -ItemType Directory -Force -Path (Join-Path $RepoRoot 'release') | Out-Null

if ($PackageOnly) {
    Write-Host '[INFO] Package-only mode: skipping compiler checks, template update, configure, and build.'
    & (Join-Path $PSScriptRoot 'package-windows.ps1') `
        -StageRoot $StageRoot -Version $Version -Configuration $Configuration
    return
}

$envInfo = & (Join-Path $PSScriptRoot 'check-windows-build-env.ps1') -VisualStudio $VisualStudio
$selected = $envInfo | Where-Object { $_.CMakePreset } | Select-Object -Last 1
$Preset = $selected.CMakePreset
$BuildPreset = $selected.BuildPreset
$BuildDir = $selected.BuildDir
$BuildDirFull = Join-Path $TemplateDir $BuildDir
$CMakeCache = Join-Path $BuildDirFull 'CMakeCache.txt'

New-Item -ItemType Directory -Force -Path $BuildRoot | Out-Null

if (-not (Test-Path -LiteralPath $TemplateDir -PathType Container)) {
    Write-Host 'Cloning official OBS plugin template (first build only)...'
    Invoke-Logged git @(
        'clone', '--depth', '1', '--branch', $TemplateRef,
        'https://github.com/obsproject/obs-plugintemplate.git',
        $TemplateDir
    ) $BuildRoot
} elseif ($RefreshTemplate) {
    Write-Host 'Refreshing official OBS plugin template...'
    Invoke-Logged git @('fetch', '--depth', '1', 'origin', $TemplateRef) $TemplateDir
    Invoke-Logged git @('checkout', 'FETCH_HEAD') $TemplateDir
} else {
    Write-Host '[FAST] Reusing cached OBS plugin template and downloaded dependencies.'
}

Write-Host 'Overlaying ArZoom source into template workspace...'
$OverlayItems = @('src', 'data', 'CMakeLists.txt', 'CMakePresets.json', 'buildspec.json')
foreach ($item in $OverlayItems) {
    $src = Join-Path $RepoRoot $item
    $dst = Join-Path $TemplateDir $item
    if (Test-Path -LiteralPath $dst) { Remove-Item -LiteralPath $dst -Recurse -Force }
    Copy-Item -LiteralPath $src -Destination $dst -Recurse -Force
}

if ($Reconfigure -or -not (Test-Path -LiteralPath $CMakeCache -PathType Leaf)) {
    Invoke-Logged cmake @('--preset', $Preset) $TemplateDir
} else {
    Write-Host '[FAST] Existing CMake configuration found; skipping dependency configure/build.'
}

Invoke-Logged cmake @('--build', '--preset', $BuildPreset, '--config', $Configuration, '--parallel') $TemplateDir

if (Test-Path -LiteralPath $StageRoot) { Remove-Item -LiteralPath $StageRoot -Recurse -Force }
New-Item -ItemType Directory -Force -Path $StageRoot | Out-Null

Invoke-Logged cmake @('--install', $BuildDir, '--prefix', $StageRoot, '--config', $Configuration) $TemplateDir

if (-not $NoPackage) {
    & (Join-Path $PSScriptRoot 'package-windows.ps1') `
        -StageRoot $StageRoot -Version $Version -Configuration $Configuration
}

Write-Host ''
Write-Host 'Build completed.'
Write-Host "Stage: $StageRoot"
Write-Host "Release folder: $(Join-Path $RepoRoot 'release')"
