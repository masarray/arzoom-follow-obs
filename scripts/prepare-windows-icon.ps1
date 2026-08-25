[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$PartsDir = Join-Path $RepoRoot 'packaging/windows/brand-parts'
$PngPath = Join-Path $RepoRoot 'packaging/windows/arzoom-brand-48.png'
$IconPath = Join-Path $RepoRoot 'packaging/windows/arzoom.ico'
$ExpectedPngBytes = 5243
$ExpectedPngSha256 = '65fde9a0fdefda5dd4376810e1c61dcfe9890ad3aedfd20ed5157f451fc986a0'

$parts = @(Get-ChildItem -LiteralPath $PartsDir -Filter 'arzoom-brand-48.png.b64.*' -File | Sort-Object Name)
if ($parts.Count -ne 5) {
    throw "Expected exactly 5 ArZoom brand chunks, found $($parts.Count)."
}

$base64 = ($parts | ForEach-Object { (Get-Content -LiteralPath $_.FullName -Raw).Trim() }) -join ''
$pngBytes = [Convert]::FromBase64String($base64)
[System.IO.File]::WriteAllBytes($PngPath, $pngBytes)

if ($pngBytes.Length -ne $ExpectedPngBytes) {
    throw "ArZoom brand PNG length mismatch: expected $ExpectedPngBytes, got $($pngBytes.Length)."
}
$pngHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $PngPath).Hash.ToLowerInvariant()
if ($pngHash -ne $ExpectedPngSha256) {
    throw "ArZoom brand PNG SHA256 mismatch: expected $ExpectedPngSha256, got $pngHash."
}

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class ArZoomNativeIcon {
    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern bool DestroyIcon(IntPtr handle);
}
'@

$stream = New-Object System.IO.MemoryStream(,$pngBytes)
$source = [System.Drawing.Bitmap]::FromStream($stream)
$icon = $null
$handle = [IntPtr]::Zero
try {
    if ($source.Width -ne 48 -or $source.Height -ne 48) {
        throw "ArZoom brand source must be exactly 48x48; got $($source.Width)x$($source.Height)."
    }

    # Use Windows/GDI+ itself to create the HICON, then serialize that HICON through
    # System.Drawing.Icon.Save. This avoids hand-written ICO containers and ensures
    # the resource is native-Windows compatible before Inno Setup sees it.
    $handle = $source.GetHicon()
    if ($handle -eq [IntPtr]::Zero) {
        throw 'Windows failed to create an HICON from the ArZoom brand bitmap.'
    }

    $icon = [System.Drawing.Icon]::FromHandle($handle)
    $file = [System.IO.File]::Open($IconPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
    try {
        $icon.Save($file)
    } finally {
        $file.Dispose()
    }

    # Parse the written file again using the Windows icon decoder as a hard gate.
    $verify = New-Object System.Drawing.Icon($IconPath)
    try {
        if ($verify.Width -le 0 -or $verify.Height -le 0) {
            throw 'Generated ArZoom icon decoded with invalid dimensions.'
        }
        Write-Host "[OK] Windows icon decoder accepted ArZoom ICO: $($verify.Width)x$($verify.Height)"
    } finally {
        $verify.Dispose()
    }

    $iconHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $IconPath).Hash.ToLowerInvariant()
    $iconBytes = (Get-Item -LiteralPath $IconPath).Length
    Write-Host "[OK] ArZoom brand source integrity: $ExpectedPngBytes bytes | SHA256 $pngHash"
    Write-Host "[OK] Generated native Windows ICO: $iconBytes bytes | SHA256 $iconHash"
} finally {
    if ($icon) { $icon.Dispose() }
    if ($handle -ne [IntPtr]::Zero) { [void][ArZoomNativeIcon]::DestroyIcon($handle) }
    if ($source) { $source.Dispose() }
    $stream.Dispose()
}
