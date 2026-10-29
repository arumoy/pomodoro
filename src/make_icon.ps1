# make_icon.ps1  (lives in src\, operates on the project root)
# Convert resources\tomato.png into a multi-resolution Windows .ico
# (resources\tomato.ico). Sizes 16/32/48/64/128/256 are packed; the 256 px
# entry is stored as embedded PNG per the modern ICO convention.
param(
    [string]$Source,
    [string]$Dest
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

# Resolve paths relative to the project root (this script's parent directory)
# so the script works no matter what the current directory is.
$root = Split-Path -Parent $PSScriptRoot
if (-not $root) { $root = (Get-Location).Path }
if (-not $Source) { $Source = Join-Path $root "resources\tomato.png" }
if (-not $Dest)   { $Dest   = Join-Path $root "resources\tomato.ico" }

$srcPath = (Resolve-Path $Source).Path
$src = [System.Drawing.Bitmap]::FromFile($srcPath)

$sizes = 16, 32, 48, 64, 128, 256

# Render each size into a PNG byte blob (high-quality resample).
$images = @()
foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap $s, $s
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode      = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.DrawImage($src, 0, 0, $s, $s)
    $g.Dispose()

    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    $images += ,@{ Size = $s; Bytes = $ms.ToArray() }
    $ms.Dispose()
}
$src.Dispose()

# Build the ICO file manually.
#   ICONDIR (6 bytes) + ICONDIRENTRY (16 bytes each) + image data.
$fs = New-Object System.IO.FileStream $Dest, ([System.IO.FileMode]::Create)
$bw = New-Object System.IO.BinaryWriter $fs
try {
    # ICONDIR
    $bw.Write([UInt16]0)                 # reserved
    $bw.Write([UInt16]1)                 # type = 1 (icon)
    $bw.Write([UInt16]$images.Count)     # image count

    # Offset where image data begins: header + entries.
    $offset = 6 + (16 * $images.Count)

    # ICONDIRENTRY per image.
    foreach ($img in $images) {
        $s = $img.Size
        $dim = if ($s -ge 256) { 0 } else { $s }  # 0 means 256
        $bw.Write([Byte]$dim)            # width
        $bw.Write([Byte]$dim)            # height
        $bw.Write([Byte]0)               # color count (0 = >=8bpp)
        $bw.Write([Byte]0)               # reserved
        $bw.Write([UInt16]1)             # color planes
        $bw.Write([UInt16]32)            # bits per pixel
        $bw.Write([UInt32]$img.Bytes.Length)  # image data size
        $bw.Write([UInt32]$offset)       # offset of image data
        $offset += $img.Bytes.Length
    }

    # Image data blobs (PNG payloads).
    foreach ($img in $images) {
        $bw.Write($img.Bytes)
    }
} finally {
    $bw.Flush()
    $bw.Dispose()
    $fs.Dispose()
}

Write-Host "[INFO] Wrote $Dest ($((Get-Item $Dest).Length) bytes) with sizes: $($sizes -join ', ')"
