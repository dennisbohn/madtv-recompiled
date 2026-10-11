# Erzeugt src\madtv.ico (Retro-Fernseher) als PNG-basiertes ICO mit mehreren Größen.
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = "Stop"

function New-TvPng([int]$size) {
    $bmp = New-Object Drawing.Bitmap $size, $size
    $g = [Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = "AntiAlias"
    $s = $size / 64.0
    function R($x, $y, $w, $h) { New-Object Drawing.RectangleF ($x * $s), ($y * $s), ($w * $s), ($h * $s) }

    # Antenne
    $pen = New-Object Drawing.Pen ([Drawing.Color]::FromArgb(60, 60, 60)), ([Math]::Max(1, 3 * $s))
    $g.DrawLine($pen, 32 * $s, 16 * $s, 20 * $s, 3 * $s)
    $g.DrawLine($pen, 32 * $s, 16 * $s, 44 * $s, 3 * $s)
    # Gehäuse
    $g.FillRectangle((New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(150, 40, 30))), (R 3 15 58 44))
    $g.FillRectangle((New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(200, 70, 45))), (R 5 17 54 40))
    # Bildschirm
    $g.FillRectangle((New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(25, 30, 40))), (R 9 21 36 32))
    $grad = New-Object Drawing.Drawing2D.LinearGradientBrush (R 11 23 32 28), ([Drawing.Color]::FromArgb(90, 200, 255)), ([Drawing.Color]::FromArgb(40, 90, 200)), 45
    $g.FillRectangle($grad, (R 11 23 32 28))
    # Knöpfe
    $knob = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(240, 210, 90))
    $g.FillEllipse($knob, (R 48 24 8 8))
    $g.FillEllipse($knob, (R 48 37 8 8))
    $g.Dispose()

    $ms = New-Object IO.MemoryStream
    $bmp.Save($ms, [Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    return , $ms.ToArray()
}

$sizes = 256, 48, 32, 16
$pngs = $sizes | ForEach-Object { , (New-TvPng $_) }

$out = New-Object IO.MemoryStream
$w = New-Object IO.BinaryWriter $out
$w.Write([UInt16]0); $w.Write([UInt16]1); $w.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $dim = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] }
    $w.Write([byte]$dim); $w.Write([byte]$dim); $w.Write([byte]0); $w.Write([byte]0)
    $w.Write([UInt16]1); $w.Write([UInt16]32)
    $w.Write([UInt32]$pngs[$i].Length); $w.Write([UInt32]$offset)
    $offset += $pngs[$i].Length
}
foreach ($p in $pngs) { $w.Write($p) }
[IO.File]::WriteAllBytes((Join-Path $PSScriptRoot "madtv.ico"), $out.ToArray())
