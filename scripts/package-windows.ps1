[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $StageRoot,

    [string] $Version = '0.1.3',
    [string] $Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$ReleaseRoot = Join-Path $RepoRoot 'release'
$PackageName = "arzoom-obs-v$Version-windows-x64"
$PackageRoot = Join-Path $ReleaseRoot $PackageName
$ZipPath = Join-Path $ReleaseRoot "$PackageName.zip"

$StageRoot = [System.IO.Path]::GetFullPath($StageRoot)
if (-not (Test-Path -LiteralPath $StageRoot -PathType Container)) {
    throw "Stage folder does not exist: $StageRoot"
}

if (Test-Path -LiteralPath $PackageRoot) {
    Remove-Item -LiteralPath $PackageRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path (Join-Path $PackageRoot 'obs-plugins/64bit') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $PackageRoot 'data/obs-plugins/arzoom') | Out-Null

# The official OBS plugin template currently installs Windows plugins as:
#   <prefix>\arzoom\bin\64bit\arzoom.dll
#   <prefix>\arzoom\data\...
# Older/direct libobs layouts use obs-plugins/64bit and data/obs-plugins.
# Support both layouts, then fall back to a bounded recursive lookup.
$DllCandidates = @(
    (Join-Path $StageRoot 'arzoom/bin/64bit/arzoom.dll'),
    (Join-Path $StageRoot 'obs-plugins/64bit/arzoom.dll'),
    (Join-Path $StageRoot 'bin/64bit/arzoom.dll')
)
$Dll = $DllCandidates |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1

if (-not $Dll) {
    $DllItem = Get-ChildItem -LiteralPath $StageRoot -Recurse -File -Filter 'arzoom.dll' |
        Select-Object -First 1
    if ($DllItem) { $Dll = $DllItem.FullName }
}
if (-not $Dll) {
    throw "arzoom.dll was not found under: $StageRoot"
}
Copy-Item -LiteralPath $Dll -Destination (Join-Path $PackageRoot 'obs-plugins/64bit/arzoom.dll') -Force

$DataRootCandidates = @(
    (Join-Path $StageRoot 'arzoom/data'),
    (Join-Path $StageRoot 'arzoom/share/obs/obs-plugins/arzoom'),
    (Join-Path $StageRoot 'data/obs-plugins/arzoom'),
    (Join-Path $StageRoot 'share/obs/obs-plugins/arzoom')
)
$DataRoot = $DataRootCandidates |
    Where-Object {
        (Test-Path -LiteralPath $_ -PathType Container) -and
        (Test-Path -LiteralPath (Join-Path $_ 'effects/arzoom.effect') -PathType Leaf)
    } |
    Select-Object -First 1

if (-not $DataRoot) {
    $EffectItem = Get-ChildItem -LiteralPath $StageRoot -Recurse -File -Filter 'arzoom.effect' |
        Select-Object -First 1
    if ($EffectItem) {
        $EffectsFolder = Split-Path -Parent $EffectItem.FullName
        $Candidate = Split-Path -Parent $EffectsFolder
        if (Test-Path -LiteralPath (Join-Path $Candidate 'locale') -PathType Container) {
            $DataRoot = $Candidate
        }
    }
}

if (-not $DataRoot) {
    throw "ArZoom data folder was not found under: $StageRoot"
}

Write-Host "[INFO] DLL source:  $Dll"
Write-Host "[INFO] Data source: $DataRoot"

Get-ChildItem -LiteralPath $DataRoot -Force |
    Copy-Item -Destination (Join-Path $PackageRoot 'data/obs-plugins/arzoom') -Recurse -Force

$Required = @(
    'obs-plugins/64bit/arzoom.dll',
    'data/obs-plugins/arzoom/effects/arzoom.effect',
    'data/obs-plugins/arzoom/locale/en-US.ini',
    'data/obs-plugins/arzoom/locale/id-ID.ini'
)
foreach ($relative in $Required) {
    $PathToCheck = Join-Path $PackageRoot $relative
    if (-not (Test-Path -LiteralPath $PathToCheck -PathType Leaf)) {
        throw "Required package file missing: $PathToCheck"
    }
}

@"
ArZoom - Smart Mouse Zoom for OBS

Manual installation:
1. Close OBS.
2. Copy the folders in this package into:
   C:\Program Files\obs-studio\
3. Restart OBS.
4. Add "ArZoom - Smart Mouse Zoom" to a Display Capture source.
5. Set "ArZoom - Toggle Zoom & Mouse Follow" in OBS Settings > Hotkeys.
"@ | Set-Content -LiteralPath (Join-Path $PackageRoot 'README-INSTALL.txt') -Encoding UTF8

if (Test-Path -LiteralPath $ZipPath) {
    Remove-Item -LiteralPath $ZipPath -Force
}
Compress-Archive -Path (Join-Path $PackageRoot '*') -DestinationPath $ZipPath -Force

# Verify that packaging really produced all required payload files.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$Archive = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
try {
    $Entries = @($Archive.Entries | ForEach-Object { $_.FullName.Replace('\', '/') })
    foreach ($relative in $Required) {
        $Expected = $relative.Replace('\', '/')
        if ($Expected -notin $Entries) {
            throw "Required payload is missing from ZIP: $Expected"
        }
    }
} finally {
    $Archive.Dispose()
}

Write-Host "[OK] ZIP package: $ZipPath"

$isccCandidates = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
    (Get-Command ISCC.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -ErrorAction SilentlyContinue)
) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) }

$iscc = $isccCandidates | Select-Object -First 1
if ($iscc) {
    $iss = Join-Path $RepoRoot 'packaging/windows/arzoom.iss'
    & $iscc "/DAppVersion=$Version" "/DSourceDir=$PackageRoot" "/DOutputDir=$ReleaseRoot" $iss
    if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed with exit code $LASTEXITCODE" }
    Write-Host '[OK] EXE installer created.'
} else {
    Write-Host '[INFO] Inno Setup 6 not found. ZIP was created; EXE installer skipped.'
}
