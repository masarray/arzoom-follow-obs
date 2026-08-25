[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$LogoPath = Join-Path $RepoRoot 'docs/assets/arzoom-logo.png'
$IconPath = Join-Path $RepoRoot 'packaging/windows/arzoom.ico'
$ExpectedLogoSha256 = 'e959de4478ed7dc44eb0d9aff9e0f41d557f4714a359d030816cbc1fb102f35f'
$ExpectedIconSha256 = '60049b4966e217077ed793827bf1836dc12015f33a4fee3fd17d8acfbb06f746'
$ExpectedSizes = @(16, 32, 48, 64, 128, 256)

foreach ($Path in @($LogoPath, $IconPath)) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required ArZoom brand asset is missing: $Path"
    }
}

$LogoHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $LogoPath).Hash.ToLowerInvariant()
if ($LogoHash -ne $ExpectedLogoSha256) {
    throw "Canonical ArZoom logo hash mismatch: expected $ExpectedLogoSha256, got $LogoHash."
}

$IconHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $IconPath).Hash.ToLowerInvariant()
if ($IconHash -ne $ExpectedIconSha256) {
    throw "ArZoom Windows icon hash mismatch: expected $ExpectedIconSha256, got $IconHash."
}

Add-Type -AssemblyName System.Drawing
$Logo = [System.Drawing.Image]::FromFile($LogoPath)
try {
    if ($Logo.Width -ne 1254 -or $Logo.Height -ne 1254) {
        throw "Canonical ArZoom logo must be 1254x1254; got $($Logo.Width)x$($Logo.Height)."
    }
} finally {
    $Logo.Dispose()
}

$IconBytes = [System.IO.File]::ReadAllBytes($IconPath)
if ($IconBytes.Length -lt 6 -or $IconBytes[0] -ne 0 -or $IconBytes[1] -ne 0 -or
    $IconBytes[2] -ne 1 -or $IconBytes[3] -ne 0) {
    throw 'ArZoom Windows icon does not have a valid ICO header.'
}

$Count = [BitConverter]::ToUInt16($IconBytes, 4)
if ($Count -ne $ExpectedSizes.Count) {
    throw "ArZoom Windows icon must contain $($ExpectedSizes.Count) resolutions; found $Count."
}

$ActualSizes = @()
for ($Index = 0; $Index -lt $Count; $Index++) {
    $Offset = 6 + ($Index * 16)
    if ($IconBytes.Length -lt ($Offset + 16)) {
        throw 'ArZoom Windows icon directory is truncated.'
    }
    $Width = if ($IconBytes[$Offset] -eq 0) { 256 } else { [int]$IconBytes[$Offset] }
    $Height = if ($IconBytes[$Offset + 1] -eq 0) { 256 } else { [int]$IconBytes[$Offset + 1] }
    if ($Width -ne $Height) {
        throw "ArZoom Windows icon frame must be square; found ${Width}x${Height}."
    }
    $ActualSizes += $Width
}

$ActualSizes = @($ActualSizes | Sort-Object)
if (($ActualSizes -join ',') -ne ($ExpectedSizes -join ',')) {
    throw "ArZoom Windows icon sizes mismatch: expected $($ExpectedSizes -join ', '); got $($ActualSizes -join ', ')."
}

$Verify = New-Object System.Drawing.Icon($IconPath)
try {
    if ($Verify.Width -le 0 -or $Verify.Height -le 0) {
        throw 'Windows decoded the ArZoom icon with invalid dimensions.'
    }
} finally {
    $Verify.Dispose()
}

Write-Host "[OK] Canonical ArZoom logo: 1254x1254 | SHA256 $LogoHash"
Write-Host "[OK] Windows ICO: $($ExpectedSizes -join ', ') px | SHA256 $IconHash"
