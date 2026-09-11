# Render A4@300dpi print sheets: 6cm color circles / number circles / person x2
Add-Type -AssemblyName System.Drawing

# 本脚本位于 tools/ 下，工程根 = 上一级目录
$projectRoot = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $projectRoot '素材\打印素材_输出'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$dpi = 300.0
$pxmm = $dpi / 25.4
$pageW = [int][math]::Round(210.0 * $pxmm)   # 2480
$pageH = [int][math]::Round(297.0 * $pxmm)   # 3508

function M([double]$mm) { [int][math]::Round($mm * $pxmm) }

# --- layout (mm) ---
$marginMM   = 12.0
$circleMM   = 60.0
$colPitchMM = 63.0
$rowGapMM   = 9.0
$labelMM    = 8.0
$rowPitchMM = $circleMM + $labelMM + $rowGapMM
$titleMM    = 15.0
$startYMM   = $marginMM + $titleMM          # top of first circle row

# person image
$personSrc = Join-Path $projectRoot '素材\图片素材\人像.png'
$imgWmm    = 80.0

$colors = @(
  @('红', '#FF0000'),
  @('橙', '#FF8000'),
  @('黄', '#FFD400'),
  @('绿', '#00A800'),
  @('蓝', '#0055FF'),
  @('紫', '#8000FF')
)

function New-Page {
  $bmp = New-Object System.Drawing.Bitmap($pageW, $pageH)
  $bmp.SetResolution(300.0, 300.0)   # so point-based fonts render at 300dpi (not 96)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
  $g.Clear([System.Drawing.Color]::White)
  return @($bmp, $g)
}

function Save-Page($bmp, $path) {
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
}

function Draw-Title($g, $text) {
  $f = New-Object System.Drawing.Font('Microsoft YaHei', [single]20, [System.Drawing.FontStyle]::Bold)
  $b = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 30, 30, 30))
  $g.DrawString($text, $f, $b, [single](M($marginMM)), [single](M(6)))
  $f.Dispose(); $b.Dispose()
}

function Draw-Caption($g, $text, $mmY) {
  $f = New-Object System.Drawing.Font('Microsoft YaHei', [single]15, [System.Drawing.FontStyle]::Bold)
  $b = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 30, 30, 30))
  $g.DrawString($text, $f, $b, [single](M($marginMM)), [single](M($mmY)))
  $f.Dispose(); $b.Dispose()
}

function Draw-Circle($g, $mmX, $mmY, $fillHex, $digit) {
  $x = M($mmX); $y = M($mmY); $d = M($circleMM)
  $rect = New-Object System.Drawing.RectangleF([single]$x, [single]$y, [single]$d, [single]$d)
  if ($digit) {
    $brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
  } else {
    $brush = New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml($fillHex))
  }
  $g.FillEllipse($brush, $rect)
  $brush.Dispose()
  $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 150, 150, 150), [single]1.5)
  $pen.DashStyle = [System.Drawing.Drawing2D.DashStyle]::Dash
  $g.DrawEllipse($pen, $rect)
  $pen.Dispose()
  if ($digit) {
    $ff = New-Object System.Drawing.Font('Arial', [single]120, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Point)
    $sf = New-Object System.Drawing.StringFormat
    $sf.Alignment = [System.Drawing.StringAlignment]::Center
    $sf.LineAlignment = [System.Drawing.StringAlignment]::Center
    $fb = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::Black)
    # nudge down ~6% of circle so the glyph (which sits high in the em box) looks centered
    $drop = [single]($d * 0.06)
    $drect = New-Object System.Drawing.RectangleF([single]$rect.X, ([single]$rect.Y + $drop), [single]$rect.Width, [single]$rect.Height)
    $g.DrawString($digit, $ff, $fb, $drect, $sf)
    $ff.Dispose(); $sf.Dispose(); $fb.Dispose()
  }
}

function Draw-Label($g, $mmX, $mmY, $text) {
  $f = New-Object System.Drawing.Font('Microsoft YaHei', [single]13, [System.Drawing.FontStyle]::Bold)
  $b = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 50, 50, 50))
  $sf = New-Object System.Drawing.StringFormat
  $sf.Alignment = [System.Drawing.StringAlignment]::Center
  $sf.LineAlignment = [System.Drawing.StringAlignment]::Near
  $rect = New-Object System.Drawing.RectangleF([single](M($mmX)), [single](M($mmY)), [single](M($colPitchMM)), [single](M($labelMM)))
  $g.DrawString($text, $f, $b, $rect, $sf)
  $f.Dispose(); $b.Dispose(); $sf.Dispose()
}

function Draw-Person($g, $topmm) {
  Draw-Caption $g '▲ 人员识别人像（VOC20「人」，共 2 份）' ($topmm - 9)
  $img = [System.Drawing.Image]::FromFile($personSrc)
  $imgHmm = $imgWmm / ($img.Width / $img.Height)
  $imgWpx = M($imgWmm); $imgHpx = M($imgHmm)
  $gap = 15.0
  $totalW = 2 * $imgWmm + $gap
  $left0mm = (210.0 - $totalW) / 2.0
  $g.DrawImage($img, [single](M($left0mm)), [single](M($topmm)), [single]$imgWpx, [single]$imgHpx)
  $g.DrawImage($img, [single](M($left0mm + $imgWmm + $gap)), [single](M($topmm)), [single]$imgWpx, [single]$imgHpx)
  $img.Dispose()
}

# --- build items ---
$colorItems = @()
foreach ($c in $colors) { for ($k = 0; $k -lt 3; $k++) { $colorItems += @{ type='color'; name=$c[0]; hex=$c[1] } } }
$numItems = @()
for ($n = 1; $n -le 4; $n++) { for ($k = 0; $k -lt 3; $k++) { $numItems += @{ type='num'; name=('数字 '+$n); digit=[string]$n } } }

$perPage = 9
$pages = @()
$idx = 1

function Render-GridPage($items, $title, $pageIndex, $personTop) {
  $bmp, $g = New-Page
  Draw-Title $g $title
  for ($i = 0; $i -lt $items.Count; $i++) {
    $col = $i % 3
    $row = [math]::Floor($i / 3)
    $mmX = $marginMM + $col * $colPitchMM
    $mmY = $startYMM + $row * $rowPitchMM
    $it = $items[$i]
    if ($it.type -eq 'color') {
      Draw-Circle $g $mmX $mmY $it.hex $null
    } else {
      Draw-Circle $g $mmX $mmY $null $it.digit
    }
    Draw-Label $g $mmX ($mmY + $circleMM + 1) $it.name
  }
  if ($personTop -ne $null) { Draw-Person $g $personTop }
  $path = Join-Path $outDir ('page{0}.png' -f $pageIndex)
  Save-Page $bmp $path
  $g.Dispose()
  return $path
}

# color pages
$colorPages = [math]::Ceiling($colorItems.Count / $perPage)
foreach ($p in 0..($colorPages - 1)) {
  $slice = $colorItems | Select-Object -Skip ($p * $perPage) -First $perPage
  $pages += (Render-GridPage $slice ('颜色识别圆（直径6cm，每种3个）  第{0}/{1}页' -f ($p+1), $colorPages) $idx $null)
  $idx++
}

# number pages (person merged onto the LAST page, below the circles)
$numPages = [math]::Ceiling($numItems.Count / $perPage)
foreach ($p in 0..($numPages - 1)) {
  $slice = $numItems | Select-Object -Skip ($p * $perPage) -First $perPage
  $personTop = $null
  if ($p -eq ($numPages - 1)) {
    $rowsUsed = [math]::Ceiling($slice.Count / 3)
    $personTop = $startYMM + $rowsUsed * $rowPitchMM + 3
  }
  $pages += (Render-GridPage $slice ('数字盘 + 人员识别人像（直径6cm，黑色加粗，每个数字3个）  第{0}/{1}页' -f ($p+1), $numPages) $idx $personTop)
  $idx++
}

Write-Output "Rendered pages:"
for ($i = 0; $i -lt $pages.Count; $i++) { Write-Output ("  page{0}.png  {1} bytes" -f ($i+1), (Get-Item ($pages[$i])).Length) }
Write-Output ("Total pages: " + $pages.Count)
