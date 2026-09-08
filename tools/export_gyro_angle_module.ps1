$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$exportRoot = Join-Path $repoRoot 'manual-build-module-export'
$packageName = 'gyro-angle-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$packageRoot = Join-Path $exportRoot $packageName
$zipPath = Join-Path $exportRoot ($packageName + '.zip')
$files = [ordered]@{
  'Core/Inc/gyro_turn.h' = 'Core/Inc/gyro_turn.h'
  'Core/Inc/mpu6050_yaw.h' = 'Core/Inc/mpu6050_yaw.h'
  'Core/Src/gyro_turn.c' = 'Core/Src/gyro_turn.c'
  'Core/Src/mpu6050_yaw.c' = 'Core/Src/mpu6050_yaw.c'
  'Core/Src/mpu6050_bus.c' = 'Core/Src/mpu6050_bus.c'
  'porting/drive_base.h' = 'Core/Inc/drive_base.h'
  'README.md' = 'reusable/gyro_angle/README.md'
  'reusable/gyro_angle/examples/angle_mode_example.h' = 'reusable/gyro_angle/examples/angle_mode_example.h'
  'reusable/gyro_angle/examples/angle_mode_example.c' = 'reusable/gyro_angle/examples/angle_mode_example.c'
  # Standalone module package has no product main.c or real DriveBase sources.
  'tests/gyro_turn/run.cmd' = 'tests/gyro_turn/run_module.cmd'
  'tests/gyro_turn/test_gyro_turn.c' = 'tests/gyro_turn/test_gyro_turn.c'
  'tests/gyro_turn/test_bus.c' = 'tests/gyro_turn/test_bus.c'
  'tests/gyro_turn/bus_stubs/main.h' = 'tests/gyro_turn/bus_stubs/main.h'
  'tests/line_recovery/stubs/main.h' = 'tests/line_recovery/stubs/main.h'
  # Tests exercise the existing bypass facade too; it is optional for consumers.
  'Core/Inc/line_bypass_turn.h' = 'Core/Inc/line_bypass_turn.h'
  'Core/Src/line_bypass_turn.c' = 'Core/Src/line_bypass_turn.c'
}
$head = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve source commit' }
$dirty = @(& git -C $repoRoot status --porcelain)
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect source state' }
$records = @()
foreach ($entry in $files.GetEnumerator()) {
  $source = Join-Path $repoRoot $entry.Value
  $destination = Join-Path $packageRoot $entry.Key
  New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
  Copy-Item -LiteralPath $source -Destination $destination
  $records += [ordered]@{
    path = $entry.Key
    source_path = $entry.Value
    bytes = (Get-Item -LiteralPath $destination).Length
    sha256 = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
  }
}
# The exported reference header is also needed on the host-test include path.
$records += [ordered]@{
  path = 'Core/Inc/drive_base.h'
  source_path = 'Core/Inc/drive_base.h'
  bytes = (Get-Item -LiteralPath (Join-Path $repoRoot 'Core/Inc/drive_base.h')).Length
  sha256 = (Get-FileHash -LiteralPath (Join-Path $repoRoot 'Core/Inc/drive_base.h') -Algorithm SHA256).Hash
}
Copy-Item -LiteralPath (Join-Path $repoRoot 'Core/Inc/drive_base.h') -Destination (Join-Path $packageRoot 'Core/Inc/drive_base.h')
$manifest = [ordered]@{
  api = 'GyroTurn v1'
  source_commit = $head
  source_dirty = ($dirty.Count -ne 0)
  created_at = (Get-Date).ToString('o')
  scope = 'Reusable sources, examples and host tests; requires target HAL/CMSIS and DriveBase implementation. No firmware or hardware validation.'
  files = $records
}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $packageRoot 'manifest.json') -Encoding UTF8
Compress-Archive -Path (Join-Path $packageRoot '*') -DestinationPath $zipPath
# Verify ZIP contents without extracting or overwriting any existing path.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($zipPath)
try {
  foreach ($record in $records) {
    $zipEntry = $archive.Entries | Where-Object { ($_.FullName -replace '\\', '/') -eq $record.path }
    if ($null -eq $zipEntry -or $zipEntry.Length -ne $record.bytes) { throw "ZIP entry missing or wrong length: $($record.path)" }
    $stream = $zipEntry.Open()
    $hasher = [Security.Cryptography.SHA256]::Create()
    try { $hash = [BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-', '') }
    finally { $hasher.Dispose(); $stream.Dispose() }
    if ($hash -ne $record.sha256) { throw "ZIP hash mismatch: $($record.path)" }
  }
}
finally { $archive.Dispose() }
Write-Output "PACKAGE: $packageRoot"
Write-Output "ZIP: $zipPath"
Write-Output "SOURCE: $head (dirty=$($manifest.source_dirty))"
Write-Output "VERIFIED: $($records.Count) file hashes"
