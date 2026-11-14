$ErrorActionPreference = 'Stop'

# Validate all bundled raster cursors. Saved choices remain independent;
# Premium Hand Chrome remains the new-filter default.
$assets = @(
    @{
        File = 'premium-hand.png'
        Sha256 = '691d368d833bd78c9a186589ddf125be5007eba8eae603a9c970a7efe526b626'
        MaxBytes = 16384
        HotspotX = 29
        HotspotY = 17
    },
    @{
        File = 'premium-hand-chrome.png'
        Sha256 = '6882dbced932ea2d12e1496f8f568b6b15ff849196832c4dc4001bec06ff86eb'
        MaxBytes = 65536
        HotspotX = 38
        HotspotY = 11
    },
    @{
        File = 'premium-chrome-arrow.png'
        Sha256 = '4ee9801f59e2e3a6157839874c1777b81331ef0082bc172fc044ec3b445a073f'
        MaxBytes = 16384
        HotspotX = 35
        HotspotY = 6
    }
)

Add-Type -AssemblyName System.Drawing
foreach ($asset in $assets) {
    $path = Join-Path $PSScriptRoot "../data/cursors/$($asset.File)"
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing bundled cursor image: $($asset.File)"
    }
    $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path $path))
    if ($bytes.Length -lt 512 -or $bytes.Length -gt $asset.MaxBytes) {
        throw "Oversized or invalid cursor $($asset.File): $($bytes.Length) bytes"
    }
    $sha = [System.Convert]::ToHexString(
        [System.Security.Cryptography.SHA256]::HashData($bytes)
    ).ToLowerInvariant()
    if ($sha -cne $asset.Sha256) {
        throw "Unexpected cursor bytes or unapproved artwork: $($asset.File)"
    }
    $stream = [System.IO.MemoryStream]::new($bytes, $false)
    try {
        $image = [System.Drawing.Bitmap]::new($stream)
        try {
            if ($image.Width -ne 160 -or $image.Height -ne 160) {
                throw "$($asset.File) must match the 160x160 GPU atlas cells"
            }
            if ($image.GetPixel(0, 0).A -ne 0) {
                throw "$($asset.File) has an opaque upper-left background"
            }
            if ($image.GetPixel(159, 159).A -ne 0) {
                throw "$($asset.File) has an opaque lower-right background"
            }
            if ($image.GetPixel($asset.HotspotX, $asset.HotspotY).A -lt 120) {
                throw "$($asset.File) hotspot no longer touches the cursor tip"
            }
        } finally {
            $image.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
    Write-Host "[PASS] $($asset.File): $($bytes.Length) bytes, transparent 160x160, hotspot ($($asset.HotspotX),$($asset.HotspotY))"
}
