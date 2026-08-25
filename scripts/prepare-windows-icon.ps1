[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$SourceBase64 = Join-Path $RepoRoot 'packaging/windows/arzoom-brand-128.png.b64'
$OutputIcon = Join-Path $RepoRoot 'packaging/windows/arzoom.ico'

if (-not (Test-Path -LiteralPath $SourceBase64 -PathType Leaf)) {
    throw "ArZoom brand source is missing: $SourceBase64"
}

Add-Type -AssemblyName System.Drawing

$pngBytes = [Convert]::FromBase64String((Get-Content -LiteralPath $SourceBase64 -Raw).Trim())
$pngStream = New-Object System.IO.MemoryStream(,$pngBytes)
$source = [System.Drawing.Bitmap]::FromStream($pngStream)

function New-ArZoomDibPayload([System.Drawing.Bitmap] $Source, [int] $Size) {
    $bitmap = New-Object System.Drawing.Bitmap($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage($Source, 0, 0, $Size, $Size)
    } finally {
        $graphics.Dispose()
    }

    $stream = New-Object System.IO.MemoryStream
    $writer = New-Object System.IO.BinaryWriter($stream)
    try {
        # BITMAPINFOHEADER. ICO DIB height includes XOR + AND masks, so it is doubled.
        $writer.Write([UInt32]40)
        $writer.Write([Int32]$Size)
        $writer.Write([Int32]($Size * 2))
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]32)
        $writer.Write([UInt32]0)
        $writer.Write([UInt32]($Size * $Size * 4))
        $writer.Write([Int32]0)
        $writer.Write([Int32]0)
        $writer.Write([UInt32]0)
        $writer.Write([UInt32]0)

        # DIB pixels are bottom-up BGRA.
        for ($y = $Size - 1; $y -ge 0; --$y) {
            for ($x = 0; $x -lt $Size; ++$x) {
                $pixel = $bitmap.GetPixel($x, $y)
                $writer.Write([Byte]$pixel.B)
                $writer.Write([Byte]$pixel.G)
                $writer.Write([Byte]$pixel.R)
                $writer.Write([Byte]$pixel.A)
            }
        }

        # 1-bit AND mask, DWORD-padded rows. Alpha already carries transparency;
        # set only genuinely transparent pixels in the compatibility mask.
        $maskRowBytes = [int](([Math]::Ceiling($Size / 32.0)) * 4)
        for ($y = $Size - 1; $y -ge 0; --$y) {
            $row = New-Object byte[] $maskRowBytes
            for ($x = 0; $x -lt $Size; ++$x) {
                if ($bitmap.GetPixel($x, $y).A -lt 128) {
                    $byteIndex = [int]($x / 8)
                    $bit = 0x80 -shr ($x % 8)
                    $row[$byteIndex] = $row[$byteIndex] -bor $bit
                }
            }
            $writer.Write($row)
        }

        $writer.Flush()
        return $stream.ToArray()
    } finally {
        $writer.Dispose()
        $stream.Dispose()
        $bitmap.Dispose()
    }
}

try {
    # Conservative Windows-native ICO frames. Inno Setup receives DIB-backed frames,
    # not PNG-in-ICO entries, avoiding the invalid-resource failure found at release audit.
    $sizes = @(16, 32, 48, 64, 128)
    $payloads = @()
    foreach ($size in $sizes) {
        $payloads += ,(New-ArZoomDibPayload -Source $source -Size $size)
    }

    $iconStream = New-Object System.IO.MemoryStream
    $iconWriter = New-Object System.IO.BinaryWriter($iconStream)
    try {
        $iconWriter.Write([UInt16]0)
        $iconWriter.Write([UInt16]1)
        $iconWriter.Write([UInt16]$sizes.Count)

        $offset = 6 + (16 * $sizes.Count)
        for ($i = 0; $i -lt $sizes.Count; ++$i) {
            $size = $sizes[$i]
            $payload = $payloads[$i]
            $iconWriter.Write([Byte]$size)
            $iconWriter.Write([Byte]$size)
            $iconWriter.Write([Byte]0)
            $iconWriter.Write([Byte]0)
            $iconWriter.Write([UInt16]1)
            $iconWriter.Write([UInt16]32)
            $iconWriter.Write([UInt32]$payload.Length)
            $iconWriter.Write([UInt32]$offset)
            $offset += $payload.Length
        }

        foreach ($payload in $payloads) {
            $iconWriter.Write($payload)
        }
        $iconWriter.Flush()
        [System.IO.File]::WriteAllBytes($OutputIcon, $iconStream.ToArray())
    } finally {
        $iconWriter.Dispose()
        $iconStream.Dispose()
    }

    $bytes = [System.IO.File]::ReadAllBytes($OutputIcon)
    if ($bytes.Length -lt 1024 -or $bytes[0] -ne 0 -or $bytes[1] -ne 0 -or $bytes[2] -ne 1 -or $bytes[3] -ne 0) {
        throw 'Generated ArZoom icon failed ICO header validation.'
    }

    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $OutputIcon).Hash.ToLowerInvariant()
    Write-Host "[OK] Generated native ArZoom Windows icon: $OutputIcon"
    Write-Host "[OK] ICO frames: $($sizes -join ', ') px | SHA256: $hash"
} finally {
    if ($source) { $source.Dispose() }
    $pngStream.Dispose()
}
