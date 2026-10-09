# Gera resources/tonal.ico: o logo do Tonal (círculo azul com barras de onda sonora pretas, igual ao Logo.qml)
# nas cores do app. Desenha cada tamanho com GDI+ (System.Drawing) e empacota os PNGs num .ico.
# Uso: powershell -ExecutionPolicy Bypass -File resources\make-icon.ps1 [-Preview <pasta>]
param([string]$Preview = "")

Add-Type -AssemblyName System.Drawing

$blue  = [System.Drawing.ColorTranslator]::FromHtml("#0D80BF")   # azul de destaque (Theme.accent)
$black = [System.Drawing.Color]::Black                            # Theme.textOnAccent
# Mesmas proporções do Logo.qml: alturas das barras, largura 9% e espaço 7% do diâmetro.
$heights = 0.32, 0.56, 0.42, 0.24

function New-IconBitmap([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)

    # Círculo azul ocupando o ícone inteiro (sem aro: na barra de tarefas o aro branco ficava pesado).
    $g.FillEllipse((New-Object System.Drawing.SolidBrush $blue), 0, 0, $size, $size)

    # Barras pretas. Nos tamanhos pequenos, largura e espaço em pixels inteiros para não borrar.
    $small = $size -le 32
    $w   = if ($small) { [Math]::Max(2, [Math]::Round($size * 0.09)) } else { $size * 0.09 }
    $gap = if ($small) { [Math]::Max(1, [Math]::Round($size * 0.07)) } else { $size * 0.07 }
    $total = 4 * $w + 3 * $gap
    $x = ($size - $total) / 2
    if ($small) { $x = [Math]::Round($x) }
    $brush = New-Object System.Drawing.SolidBrush $black
    foreach ($h in $heights) {
        $bh = $size * $h
        if ($small) { $bh = [Math]::Max($w, [Math]::Round($bh)) }
        $y = ($size - $bh) / 2
        if ($small) { $y = [Math]::Round($y) }
        $path = New-Object System.Drawing.Drawing2D.GraphicsPath
        $r = $w   # diâmetro dos cantos = largura da barra (pontas arredondadas)
        $path.AddArc($x, $y, $r, $r, 180, 180)
        $path.AddArc($x, $y + $bh - $r, $r, $r, 0, 180)
        $path.CloseFigure()
        $g.FillPath($brush, $path)
        $x += $w + $gap
    }
    $g.Dispose()
    return $bmp
}

$sizes = 16, 20, 24, 32, 40, 48, 64, 128, 256
$pngs = @()
foreach ($s in $sizes) {
    $bmp = New-IconBitmap $s
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngs += , $ms.ToArray()
    if ($Preview) { $bmp.Save((Join-Path $Preview "icon-$s.png"), [System.Drawing.Imaging.ImageFormat]::Png) }
    $bmp.Dispose()
}

# Formato ICO: ICONDIR (6 bytes) + uma ICONDIRENTRY (16 bytes) por tamanho + as imagens (PNG).
$out = Join-Path $PSScriptRoot "tonal.ico"
$fs = [System.IO.File]::Create($out)
$bw = New-Object System.IO.BinaryWriter $fs
$bw.Write([UInt16]0); $bw.Write([UInt16]1); $bw.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $dim = if ($s -ge 256) { 0 } else { $s }   # 0 significa 256
    $bw.Write([Byte]$dim); $bw.Write([Byte]$dim); $bw.Write([Byte]0); $bw.Write([Byte]0)
    $bw.Write([UInt16]1); $bw.Write([UInt16]32)
    $bw.Write([UInt32]$pngs[$i].Length); $bw.Write([UInt32]$offset)
    $offset += $pngs[$i].Length
}
foreach ($p in $pngs) { $bw.Write($p) }
$bw.Dispose()
"Gerado: $out ($((Get-Item $out).Length) bytes, tamanhos $($sizes -join ', '))"
