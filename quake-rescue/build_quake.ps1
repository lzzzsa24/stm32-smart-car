# Earthquake rescue car - standalone STM32 build (GNU ARM via STM32CubeIDE toolchain)
# Output: manual-build-quake\quake_earthquake.hex
$ErrorActionPreference = 'Stop'

$projectRoot = $PSScriptRoot

# STM32CubeIDE 1.16.0 bundled GNU ARM toolchain (this machine)
$toolRoot = 'D:\Programs\ST\STM32CubeIDE_1.16.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.win32_1.0.200.202406191623\tools\bin'
$gcc      = Join-Path $toolRoot 'arm-none-eabi-gcc.exe'
$objcopy  = Join-Path $toolRoot 'arm-none-eabi-objcopy.exe'
$size     = Join-Path $toolRoot 'arm-none-eabi-size.exe'

if (-not (Test-Path -LiteralPath $gcc)) {
  throw "STM32 GCC not found: $gcc (check STM32CubeIDE install path/version)"
}

$buildDir     = Join-Path $projectRoot 'manual-build-quake'
$artifactName = 'quake_earthquake'
$linker       = Join-Path $projectRoot 'STM32F103ZETX_FLASH.ld'
$startup      = Join-Path $projectRoot 'Core\StartUp\startup_stm32f103zetx.s'

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

# ---- Include paths (precomputed strings) ----
$incCore = '-I' + (Join-Path $projectRoot 'Core\Inc')
$incMotion = '-I' + (Join-Path $projectRoot 'motion')
$incQuake  = '-I' + (Join-Path $projectRoot 'quake')
$incHal    = '-I' + (Join-Path $projectRoot 'Drivers\STM32F1xx_HAL_Driver\Inc')
$incHalLegacy = '-I' + (Join-Path $projectRoot 'Drivers\STM32F1xx_HAL_Driver\Inc\Legacy')
$incCmsisDev  = '-I' + (Join-Path $projectRoot 'Drivers\CMSIS\Device\ST\STM32F1xx\Include')
$incCmsis     = '-I' + (Join-Path $projectRoot 'Drivers\CMSIS\Include')

$commonArgs = @(
  '-mcpu=cortex-m3', '-mthumb', '-mfloat-abi=soft',
  '-DDEBUG', '-DUSE_HAL_DRIVER', '-DSTM32F103xE',
  '-O0', '-g3', '-ffunction-sections', '-fdata-sections',
  '-Wall', '-fstack-usage', '-MMD', '-MP',
  $incCore, $incMotion, $incQuake,
  $incHal, $incHalLegacy, $incCmsisDev, $incCmsis
)

# ---- Platform: Core/Src (no main.c; main() is quake_main.c) ----
$platformSources = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Core\Src') -Filter '*.c' | Sort-Object Name

# ---- App: quake/ (quake_main.c is the entry; state machine + vision).
#      Only top-level .c; quake/platform/stm32f1xx_it.c is already in Core/Src. ----
$quakeSources = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'quake') -Filter '*.c' | Sort-Object Name

# ---- Motion: motion/ (experiment-7 modules; motor.c is legacy GPIO, unused, excluded) ----
$motionSources = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'motion') -Filter '*.c' |
  Where-Object { $_.Name -ne 'motor.c' } | Sort-Object Name

# ---- HAL sources ----
$halNames = @(
  'stm32f1xx_hal.c',
  'stm32f1xx_hal_cortex.c',
  'stm32f1xx_hal_dma.c',
  'stm32f1xx_hal_exti.c',
  'stm32f1xx_hal_flash.c',
  'stm32f1xx_hal_flash_ex.c',
  'stm32f1xx_hal_gpio.c',
  'stm32f1xx_hal_gpio_ex.c',
  'stm32f1xx_hal_pwr.c',
  'stm32f1xx_hal_rcc.c',
  'stm32f1xx_hal_rcc_ex.c',
  'stm32f1xx_hal_tim.c',
  'stm32f1xx_hal_tim_ex.c'
)
$halSources = $halNames | ForEach-Object { Get-Item -LiteralPath (Join-Path $projectRoot "Drivers\STM32F1xx_HAL_Driver\Src\$_") }

$sources = @($platformSources) + @($quakeSources) + @($motionSources) + @($halSources)
Write-Output ("Sources: " + $sources.Count)
$objects = @()

for ($index = 0; $index -lt $sources.Count; ++$index) {
  $source = $sources[$index]
  $object = Join-Path $buildDir ('obj_{0:D3}.o' -f $index)
  & $gcc @commonArgs '-std=gnu11' '-c' $source.FullName '-o' $object
  if ($LASTEXITCODE -ne 0) { throw "Compile failed: " + $source.FullName }
  $objects += $object
}

$startupObject = Join-Path $buildDir 'startup_stm32f103zetx.o'
& $gcc @commonArgs '-x' 'assembler-with-cpp' '-c' $startup '-o' $startupObject
if ($LASTEXITCODE -ne 0) { throw 'Startup assembly failed' }
$objects += $startupObject

$elf = Join-Path $buildDir ($artifactName + '.elf')
$map = Join-Path $buildDir ($artifactName + '.map')
$hex = Join-Path $buildDir ($artifactName + '.hex')
$bin = Join-Path $buildDir ($artifactName + '.bin')

$linkArgs = @(
  '-mcpu=cortex-m3', '-mthumb', '-mfloat-abi=soft',
  "-T$linker", '--specs=nosys.specs', '--specs=nano.specs',
  "-Wl,-Map=$map", '-Wl,--gc-sections', '-static'
) + $objects + @('-Wl,--start-group', '-lc', '-lm', '-Wl,--end-group', '-o', $elf)

& $gcc @linkArgs
if ($LASTEXITCODE -ne 0) { throw 'Link failed' }
& $objcopy '-O' 'ihex' $elf $hex
if ($LASTEXITCODE -ne 0) { throw 'HEX generation failed' }
& $objcopy '-O' 'binary' $elf $bin
if ($LASTEXITCODE -ne 0) { throw 'BIN generation failed' }

& $size $elf
Write-Output "HEX: $hex"
Write-Output "SHA256: $((Get-FileHash -Algorithm SHA256 -LiteralPath $hex).Hash)"
