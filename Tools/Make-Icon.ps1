# FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$out=Join-Path $PSScriptRoot '..\Assets'
[IO.Directory]::CreateDirectory($out) | Out-Null
$bitmap=New-Object Drawing.Bitmap 256,256
$g=[Drawing.Graphics]::FromImage($bitmap)
$g.SmoothingMode='AntiAlias'
$g.Clear([Drawing.Color]::FromArgb(20,29,41))
$g.ScaleTransform(6,6)
$g.TranslateTransform(4,7)
$mint=New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(121,230,200))
foreach($poly in @('1,10 10,3 29,3 21,10 11,10 5,16 1,16','7,19 16,12 34,12 26,19 17,19 8,28 2,28')){
    [Drawing.PointF[]]$pts=foreach($pair in $poly.Split(' ')){$x,$y=$pair.Split(',');[Drawing.PointF]::new([float]$x,[float]$y)}
    $g.FillPolygon($mint,$pts)
}
$stream=[IO.MemoryStream]::new()
$bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Png)
$icon=[IO.BinaryWriter]::new([IO.File]::Create((Join-Path $out 'FrameBridge.ico')))
try{
    $icon.Write([uint16]0);$icon.Write([uint16]1);$icon.Write([uint16]1)
    $icon.Write([uint32]0);$icon.Write([uint16]1);$icon.Write([uint16]32)
    $icon.Write([uint32]$stream.Length);$icon.Write([uint32]22);$icon.Write($stream.ToArray())
}finally{$icon.Dispose();$stream.Dispose();$mint.Dispose();$g.Dispose();$bitmap.Dispose()}
