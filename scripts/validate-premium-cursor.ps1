$ErrorActionPreference = 'Stop'
$path = Join-Path $PSScriptRoot '../data/cursors/premium-hand.png'
if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
    throw 'Bundled Premium Hand cursor image is missing.'
}
$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path $path))
if ($bytes.Length -gt 16384 -or $bytes.Length -lt 512) {
    throw "Unexpected Premium Hand size: $($bytes.Length) bytes."
}
$sha = [System.BitConverter]::ToString(
    [System.Security.Cryptography.SHA256]::HashData($bytes)
).Replace('-', '').ToLowerInvariant()
if ($sha -ne '691d368d833bd78c9a186589ddf125be5007eba8eae603a9c970a7efe526b626') {
    throw 'Bundled Premium Hand PNG does not match approved optimized artwork.'
}
Add-Type -AssemblyName System.Drawing
$stream = [System.IO.MemoryStream]::new($bytes, $false)
try {
    $image = [System.Drawing.Bitmap]::new($stream)
    try {
        if ($image.Width -ne 160 -or $image.Height -ne 160) {
            throw 'Premium Hand cursor must be 160 x 160 to fit the preset atlas.'
        }
        if ($image.GetPixel(0, 0).A -ne 0) {
            throw 'Premium Hand contains an opaque background.'
        }
        if ($image.GetPixel(29, 17).A -lt 120) {
            throw 'Premium Hand pointer hotspot is no longer at the fingertip.'
        }
    } finally {
        $image.Dispose()
    }
} finally {
    $stream.Dispose()
}
Write-Host "[PASS] Bundled Premium Hand PNG: $($bytes.Length) bytes, transparent, 160x160, fingertip hotspot."
