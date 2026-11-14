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
        Sha256 = 'dbcadd91d442485cd71c97b5c04cb1fe1eb19bd992da0d51e4da60e74cad0fa1'
        MaxBytes = 16384
        HotspotX = 25
        HotspotY = 4
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
            if ($asset.File -eq 'premium-chrome-arrow.png') {
                # A technically valid PNG can still look dirty at video scale.
                # Require a single connected, legible silhouette on both
                # dark and light backgrounds at native 32/48/64 px sizes.
                foreach ($size in @(32, 48, 64)) {
                    $small = [System.Drawing.Bitmap]::new(
                        $size, $size,
                        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
                    try {
                        $g = [System.Drawing.Graphics]::FromImage($small)
                        try {
                            $g.Clear([System.Drawing.Color]::Transparent)
                            $g.InterpolationMode =
                                [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                            $g.DrawImage($image, 0, 0, $size, $size)
                        } finally { $g.Dispose() }
                        $occupied = [bool[]]::new($size * $size)
                        $bright = 0
                        for ($y=0; $y -lt $size; ++$y) {
                            for ($x=0; $x -lt $size; ++$x) {
                                $c = $small.GetPixel($x, $y)
                                $occupied[$y*$size+$x] = $c.A -ge 64
                                if ($c.A -ge 192 -and $c.R -ge 185 -and
                                    $c.G -ge 185 -and $c.B -ge 185) {
                                    ++$bright
                                }
                            }
                        }
                        $seen = [bool[]]::new($size * $size)
                        $components = 0
                        for ($i=0; $i -lt $occupied.Length; ++$i) {
                            if (-not $occupied[$i] -or $seen[$i]) { continue }
                            ++$components
                            if ($components -ne 1) {
                                throw "Detached alpha speckle at $size px"
                            }
                            $queue = [System.Collections.Generic.Queue[int]]::new()
                            $queue.Enqueue($i); $seen[$i] = $true
                            while ($queue.Count -gt 0) {
                                $cell = $queue.Dequeue()
                                $cx = $cell % $size
                                $cy = [int][math]::Floor($cell / $size)
                                for ($dy=-1; $dy -le 1; ++$dy) {
                                    for ($dx=-1; $dx -le 1; ++$dx) {
                                        if ($dx -eq 0 -and $dy -eq 0) { continue }
                                        $nx=$cx+$dx; $ny=$cy+$dy
                                        if ($nx -lt 0 -or $ny -lt 0 -or
                                            $nx -ge $size -or $ny -ge $size) {
                                            continue
                                        }
                                        $ni=$ny*$size+$nx
                                        if ($occupied[$ni] -and -not $seen[$ni]) {
                                            $seen[$ni]=$true
                                            $queue.Enqueue($ni)
                                        }
                                    }
                                }
                            }
                        }
                        if ($components -ne 1 -or
                            $bright -lt $size * $size * 0.08) {
                            throw "Unreadable or fragmented Chrome Arrow at $size px"
                        }
                        Write-Host "[PASS] Chrome Arrow $size px: connected alpha and readable white face"
                    } finally { $small.Dispose() }
                }
            }
        } finally {
            $image.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
    Write-Host "[PASS] $($asset.File): $($bytes.Length) bytes, transparent 160x160, hotspot ($($asset.HotspotX),$($asset.HotspotY))"
}
