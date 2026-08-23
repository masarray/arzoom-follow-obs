[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $StageRoot,

    [string] $Version = '0.3.0',
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
ArZoom v$Version - Smart Zone Camera + GPU Click Visualization for OBS

RECOMMENDED: use the EXE installer.
The installer supports two modes:
  1. Standard OBS Studio - auto detected.
  2. OBS Portable / custom OBS folder - browse to the OBS root folder.

The correct OBS root folder contains:
  bin\64bit\obs64.exe

Manual ZIP installation:
1. Close OBS completely.
2. Open your OBS root folder.
   Standard example: C:\Program Files\obs-studio\
   Portable example: D:\PortableApps\OBS\
3. Copy the obs-plugins and data folders from this ZIP into that OBS root.
4. Restart OBS.
5. Add "ArZoom - Smart Camera Zoom & Follow" to a Display Capture source.
6. Set "ArZoom - Toggle Smart Camera Zoom" in OBS Settings > Hotkeys.
7. Leave "Show click visualization" enabled to display GPU-rendered left/right/middle click feedback.

Click visualization does not retarget the Smart Zone camera. It is rendered procedurally in the same presentation pass and stays anchored to clicked content while zoom/pan moves.

Do not copy the package into bin\64bit or obs-plugins directly; merge it at the OBS root.
"@ | Set-Content -LiteralPath (Join-Path $PackageRoot 'README-INSTALL.txt') -Encoding UTF8

if (Test-Path -LiteralPath $ZipPath) {
    Remove-Item -LiteralPath $ZipPath -Force
}
Compress-Archive -Path (Join-Path $PackageRoot '*') -DestinationPath $ZipPath -Force

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
