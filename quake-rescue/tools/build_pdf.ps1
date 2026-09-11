# Merge the 4 A4 page PNGs into one exact-A4 multi-page PDF (hand-built, no extra deps)
Add-Type -AssemblyName System.Drawing

# 本脚本位于 tools/ 下，工程根 = 上一级目录
$projectRoot = Split-Path -Parent $PSScriptRoot
$outDir  = Join-Path $projectRoot '素材\打印素材_输出'
$pages   = 1..4 | ForEach-Object { Join-Path $outDir ('page{0}.png' -f $_) }
$pdfPath = Join-Path $outDir '打印素材_救援彩标_4页.pdf'

# ---- encode each page as high-quality JPEG and read bytes ----
$jc = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$jpegs = New-Object System.Collections.Generic.List[byte[]]
foreach ($pg in $pages) {
  $bmp = New-Object System.Drawing.Bitmap($pg)
  $tmp = Join-Path $env:TEMP ('p{0}.jpg' -f [guid]::NewGuid().ToString('N'))
  $ep = New-Object System.Drawing.Imaging.EncoderParameters(1)
  $ep.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality, [long]95)
  $bmp.Save($tmp, $jc, $ep)
  $bmp.Dispose(); $ep.Dispose()
  $jpegs.Add([System.IO.File]::ReadAllBytes($tmp))
  Remove-Item $tmp -Force
}

$N  = $jpegs.Count
$PW = '595.28'; $PH = '841.89'   # A4 in points

$ms = New-Object System.IO.MemoryStream
$offsets = @{}
function W([string]$s) { $b=[System.Text.Encoding]::ASCII.GetBytes($s); $ms.Write($b,0,$b.Length) }
function ObjB([int]$n) { $offsets[$n]=$ms.Position; W ("$n 0 obj`n") }
function ObjE { W "`nendobj`n" }

# header (+ binary comment)
W "%PDF-1.4`n%"
foreach ($b in @(0xE2,0xE3,0xCF,0xD3)) { $ms.WriteByte([byte]$b) }
$ms.WriteByte([byte]0x0A)

# 1: catalog
ObjB 1; W "<< /Type /Catalog /Pages 2 0 R >>"; ObjE
# 2: pages
$kid = ""; for ($i=0;$i -lt $N;$i++){ $kid += ("{0} 0 R " -f (3+3*$i)) }
ObjB 2
W ("<< /Type /Pages /Kids [{0}] /Count {1} >>" -f $kid, $N)
ObjE

# page objects + resources
for ($i=0;$i -lt $N;$i++) {
  $pnum = 3+3*$i; $cnum = 4+3*$i; $inum = 5+3*$i
  ObjB $pnum
  W ("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 {0} {1}] /Resources << /XObject << /Im0 {2} 0 R >> >> /Contents {3} 0 R >>" -f $PW,$PH,$inum,$cnum)
  ObjE
  $content = "q`n{0} 0 0 {1} 0 0 cm`n/Im0 Do`nQ" -f $PW,$PH
  $clen = [System.Text.Encoding]::ASCII.GetByteCount($content)
  ObjB $cnum
  W ("<< /Length {0} >>`nstream`n" -f $clen); W $content; W "`nendstream"; ObjE
  $img = $jpegs[$i]
  ObjB $inum
  W ("<< /Type /XObject /Subtype /Image /Width 2480 /Height 3508 /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /DCTDecode /Length {0} >>`nstream`n" -f $img.Length)
  $ms.Write($img,0,$img.Length)
  W "`nendstream"; ObjE
}

# xref + trailer
$xrefPos = $ms.Position
$total = $N*3 + 2
W ("xref`n0 {0}`n" -f ($total+1))
W "0000000000 65535 f `n"
for ($n=1; $n -le $total; $n++) { W ("{0:D10} 00000 n `n" -f $offsets[$n]) }
W ("trailer`n<< /Size {0} /Root 1 0 R >>`nstartxref`n{1}`n%%EOF" -f ($total+1), $xrefPos)

[System.IO.File]::WriteAllBytes($pdfPath, $ms.ToArray())
$ms.Dispose()
Write-Output ("PDF written: " + $pdfPath + "  " + (Get-Item $pdfPath).Length + " bytes  pages=" + $pages.Count)
