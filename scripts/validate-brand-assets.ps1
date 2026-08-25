[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$ExpectedLogoSha256 = 'e959de4478ed7dc44eb0d9aff9e0f41d557f4714a359d030816cbc1fb102f35f'

function Resolve-RepoFile {
    param([Parameter(Mandatory = $true)][string]$RelativePath)

    $Path = Join-Path $RepoRoot $RelativePath
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required brand asset is missing: $RelativePath"
    }
    return $Path
}

function Assert-RasterDimensions {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][int]$Width,
        [Parameter(Mandatory = $true)][int]$Height
    )

    $Path = Resolve-RepoFile $RelativePath
    $Image = [System.Drawing.Image]::FromFile($Path)
    try {
        if ($Image.Width -ne $Width -or $Image.Height -ne $Height) {
            throw "$RelativePath must be ${Width}x${Height}; got $($Image.Width)x$($Image.Height)."
        }
    } finally {
        $Image.Dispose()
    }
}

Add-Type -AssemblyName System.Drawing

$LogoPath = Resolve-RepoFile 'docs/assets/arzoom-logo.png'
$LogoHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $LogoPath).Hash.ToLowerInvariant()
if ($LogoHash -ne $ExpectedLogoSha256) {
    throw "Canonical ArZoom logo hash mismatch: expected $ExpectedLogoSha256, got $LogoHash."
}

Assert-RasterDimensions 'docs/assets/arzoom-logo.png' 1254 1254
Assert-RasterDimensions 'docs/assets/arzoom-logo-96.png' 96 96
Assert-RasterDimensions 'docs/assets/favicon-32.png' 32 32
Assert-RasterDimensions 'docs/assets/favicon-192.png' 192 192
Assert-RasterDimensions 'docs/assets/apple-touch-icon.png' 180 180
Assert-RasterDimensions 'docs/assets/og-image.png' 1200 630
Assert-RasterDimensions 'docs/assets/product-screenshot.png' 1600 900
Assert-RasterDimensions 'data/arzoom-logo.png' 512 512

$PrepareIcon = Resolve-RepoFile 'scripts/prepare-windows-icon.ps1'
& $PrepareIcon

$PackagingIconPath = Resolve-RepoFile 'packaging/windows/arzoom.ico'
$WebsiteIconPath = Resolve-RepoFile 'docs/favicon.ico'
$PackagingIconHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $PackagingIconPath).Hash.ToLowerInvariant()
$WebsiteIconHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $WebsiteIconPath).Hash.ToLowerInvariant()
if ($WebsiteIconHash -ne $PackagingIconHash) {
    throw "Website favicon ICO must match the validated Windows icon; got $WebsiteIconHash instead of $PackagingIconHash."
}

$IndexPath = Resolve-RepoFile 'docs/index.html'
$Index = Get-Content -LiteralPath $IndexPath -Raw
$RequiredIndexFragments = @(
    'assets/favicon-32.png',
    'assets/apple-touch-icon.png',
    'assets/arzoom-logo-96.png',
    'assets/product-screenshot.png',
    'property="og:image" content="https://masarray.github.io/arzoom-follow-obs/assets/og-image.png"',
    'name="twitter:card" content="summary_large_image"'
)
foreach ($Fragment in $RequiredIndexFragments) {
    if (-not $Index.Contains($Fragment)) {
        throw "Landing page brand contract is missing: $Fragment"
    }
}

$InstallerDefinition = Get-Content -LiteralPath (Resolve-RepoFile 'packaging/windows/arzoom.iss') -Raw
if ($InstallerDefinition -notmatch 'SetupIconFile=arzoom\.ico' -or
    $InstallerDefinition -notmatch 'UninstallDisplayIcon=.*arzoom\.ico') {
    throw 'Installer definition does not use the ArZoom icon for setup and uninstall surfaces.'
}

$CMake = Get-Content -LiteralPath (Resolve-RepoFile 'CMakeLists.txt') -Raw
if ($CMake -notmatch 'packaging/windows/arzoom-icon\.rc') {
    throw 'Windows plugin binary does not embed the ArZoom app icon resource.'
}

$BuildScript = Get-Content -LiteralPath (Resolve-RepoFile 'scripts/build-local-windows.ps1') -Raw
if ($BuildScript -notmatch "'packaging'") {
    throw 'OBS template overlay does not include the Windows app icon resource directory.'
}

$PackageScript = Get-Content -LiteralPath (Resolve-RepoFile 'scripts/package-windows.ps1') -Raw
if ($PackageScript.Contains('Recommended v0.5.0') -or $PackageScript.Contains('v0.5.0 also includes')) {
    throw 'Manual ZIP copy contains a stale hard-coded v0.5.0 release label.'
}

Write-Host '[OK] Canonical logo, favicons, product screenshot, OG image, installer icon, and app icon contracts passed.'
