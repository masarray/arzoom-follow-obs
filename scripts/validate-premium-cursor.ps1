$ErrorActionPreference = 'Stop'

# Validate BOTH bundled hands. The original is preserved so saved settings
# keep rendering the previous artwork; Chrome becomes the new-filter default.
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
        Sha256 = 'cec032657b8a247c1bc5c848bc94a1b1b635e6c2d3040571bb2dde36bbc9ddee'
        MaxBytes = 65536
        HotspotX = 38
        HotspotY = 11
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
                throw "$($asset.File) hotspot no longer touches the fingertip"
            }
        } finally {
            $image.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
    Write-Host "[PASS] $($asset.File): $($bytes.Length) bytes, transparent 160x160, fingertip ($($asset.HotspotX),$($asset.HotspotY))"
}
