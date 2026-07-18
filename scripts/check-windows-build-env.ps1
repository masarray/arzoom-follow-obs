[CmdletBinding()]
param(
    [switch] $Quiet,
    [ValidateSet('Auto', '2026', '2022')]
    [string] $VisualStudio = 'Auto'
)

$ErrorActionPreference = 'Stop'

function Write-Ok([string]$Message) {
    if (-not $Quiet) { Write-Host "[OK] $Message" -ForegroundColor Green }
}
function Fail-WithHelp([string]$Message) {
    Write-Host ''
    Write-Host '[ArZoom build environment check failed]' -ForegroundColor Red
    Write-Host $Message -ForegroundColor Yellow
    Write-Host ''
    Write-Host 'Install:'
    Write-Host '  1. Git for Windows'
    Write-Host '  2. CMake 3.28 or newer'
    Write-Host '  3. Visual Studio 2026 or 2022'
    Write-Host '  4. Workload: Desktop development with C++'
    throw $Message
}
function Require-Command([string]$Name, [string]$Help) {
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $cmd) { Fail-WithHelp "$Name was not found. $Help" }
    Write-Ok "$Name found: $($cmd.Source)"
}

Require-Command git 'Install Git for Windows.'
Require-Command cmake 'Install CMake.'

$cmakeHelp = (& cmake --help) -join "`n"
$hasVs2026Generator = $cmakeHelp -match 'Visual Studio 18 2026'
$hasVs2022Generator = $cmakeHelp -match 'Visual Studio 17 2022'

$vswhereCandidates = @(
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }

$vswhere = [string]($vswhereCandidates | Select-Object -First 1)
if (-not $vswhere) { Fail-WithHelp 'Visual Studio Installer / vswhere.exe was not found.' }

function Find-Vs([string]$VersionRange) {
    (& $vswhere -latest -version $VersionRange -products * `
       -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
       -property installationPath) | Select-Object -First 1
}

$vs2026 = Find-Vs '[18.0,19.0)'
$vs2022 = Find-Vs '[17.0,18.0)'

if (($VisualStudio -eq '2026' -or $VisualStudio -eq 'Auto') -and $vs2026 -and $hasVs2026Generator) {
    $selected = [pscustomobject]@{
        VisualStudioYear = '2026'
        CMakePreset = 'windows-vs2026-x64'
        BuildPreset = 'windows-vs2026-x64'
        BuildDir = 'build_vs2026_x64'
    }
} elseif (($VisualStudio -eq '2022' -or $VisualStudio -eq 'Auto') -and $vs2022 -and $hasVs2022Generator) {
    $selected = [pscustomobject]@{
        VisualStudioYear = '2022'
        CMakePreset = 'windows-vs2022-x64'
        BuildPreset = 'windows-vs2022-x64'
        BuildDir = 'build_vs2022_x64'
    }
} else {
    Fail-WithHelp 'No compatible Visual Studio installation and CMake generator pair was found.'
}

Write-Ok "Visual Studio $($selected.VisualStudioYear) selected."
$selected
