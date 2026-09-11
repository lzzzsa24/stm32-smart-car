# Generate one single-page A4 PDF per page image (4 separate PDFs)
Add-Type -AssemblyName System.Drawing

# 本脚本位于 tools/ 下，工程根 = 上一级目录
$projectRoot = Split-Path -Parent $PSScriptRoot
$outDir  = Join-Path $projectRoot '素材\打印素材_输出'

# (pageIndex, sourcePNG, outputName)
$items = @(
  @(1, 'page1.png', '打印素材_第1页_颜色红橙黄.pdf'),
  @(2, 'page2.png', '打印素材_第2页_颜色绿蓝紫.pdf'),
  @(3, 'page3.png', '打印素材_第3页_数字123.pdf'),
  @(4, 'page4.png', '打印素材_第4页_数字4人像.pdf')
)

$jc = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$PW = '595.28'; $PH = '841.89'

function Write-OnePDF($jpegBytes, $path) {
  $ms = New-Object System.IO.MemoryStream
  $offsets = @{}
  function W([string]$s) { $b=[System.Text.Encoding]::ASCII.GetBytes($s); $ms.Write($b,0,$b.Length) }
  function ObjB([int]$n) { $offsets[$n]=$ms.Position; W ("$n 0 obj`n") }
  function ObjE { W "`nendobj`n" }

  W "%PDF-1.4`n%"
  foreach ($b in @(0xE2,0xE3,0xCF,0xD3)) { $ms.WriteByte([byte]$b) }
  $ms.WriteByte([byte]0x0A)

  ObjB 1; W "<< /Type /Catalog /Pages 2 0 R >>"; ObjE
  ObjB 2; W "<< /Type /Pages /Kids [3 0 R] /Count 1 >>"; ObjE
  ObjB 3; W ("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 {0} {1}] /Resources << /XObject << /Im0 5 0 R >> >> /Contents 4 0 R >>" -f $PW,$PH); ObjE

  $content = "q`n{0} 0 0 {1} 0 0 cm`n/Im0 Do`nQ" -f $PW,$PH
  $clen = [System.Text.Encoding]::ASCII.GetByteCount($content)
  ObjB 4; W ("<< /Length {0} >>`nstream`n" -f $clen); W $content; W "`nendstream"; ObjE

  ObjB 5
  W ("<< /Type /XObject /Subtype /Image /Width 2480 /Height 3508 /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /DCTDecode /Length {0} >>`nstream`n" -f $jpegBytes.Length)
  $ms.Write($jpegBytes,0,$jpegBytes.Length)
  W "`nendstream"; ObjE

  $xrefPos = $ms.Position
  W ("xref`n0 6`n")
  W "0000000000 65535 f `n"
  for ($n=1; $n -le 5; $n++) { W ("{0:D10} 00000 n `n" -f $offsets[$n]) }
  W ("trailer`n<< /Size 6 /Root 1 0 R >>`nstartxref`n{0}`n%%EOF" -f $xrefPos)

  [System.IO.File]::WriteAllBytes($path, $ms.ToArray())
  $ms.Dispose()
}

foreach ($it in $items) {
  $png = Join-Path $outDir $it[1]
  $pdf = Join-Path $outDir $it[2]
  $bmp = New-Object System.Drawing.Bitmap($png)
  $tmp = Join-Path $env:TEMP ('q{0}.jpg' -f [guid]::NewGuid().ToString('N'))
  $ep = New-Object System.Drawing.Imaging.EncoderParameters(1)
  $ep.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality, [long]95)
  $bmp.Save($tmp, $jc, $ep)
  $bmp.Dispose(); $ep.Dispose()
  $bytes = [System.IO.File]::ReadAllBytes($tmp)
  Remove-Item $tmp -Force
  Write-OnePDF $bytes $pdf
  Write-Output ("OK  page {0}: {1}  {2} bytes" -f $it[0], $it[2], (Get-Item $pdf).Length)
}
Write-Output ("Total PDFs: " + $items.Count)
