# Comprehensive deployment 0cdf8ee

Requested delta: a90ce1391814e39fde078dc07a60e57b7d985306.
Base: 9ba87c30c4994d54a51bf13cb27dc63b06e8d21c.
Result: 0cdf8ee5dc1798556f18626fa949117811e5f912.
Branch: test/comprehensive-v15-sign-horn-20260909.
Checkout: F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn.

Restores DriveBase and sign slowdown power sources exactly to 8b91f49:
removes the experimental powered/coast path and 500-CPS search cap, retains
normal encoder speed control. Independent ARC direction fix, 24-cm fixed
bypass and disabled obstacle IR policy remain. Side IR protection is still
absent. K210 and main firmware code unchanged; no push.

Build and sign_line, line_recovery and gyro_turn suites all exited 0.
Baseline source comparisons and git diff --check passed; source checkout clean.
ELF text/data/bss: 108096/64/18320 bytes.
Artifacts: checkout/manual-build-unified-motion/exp7_unified_motion.bin and .hex.
BIN size: 108164 bytes.
BIN SHA256: C1AB5D7FC84573A34DD02F0DC4B68BDE6B19FD5C4724AE68E6C18F4302DDC3A7
HEX SHA256: B22CAD4E866F880D86382C26C47859F6C1075D363702722AEEC976610F1EDFFB

COM11 enumerated immediately before flashing. PowerShell 7 ran
F:/myproject/jidian/tools/stm32_uart_flash.ps1 with -BaudRate 57600 and
-PreserveLastPage against this exact BIN. Exit 0:

```text
BOOTLOADER ACK: boot=DTR value=True reset-active=False
ERASE OK: 53 firmware pages; calibration page preserved
WRITE OK: 108164 bytes
VERIFY OK: 108164 bytes
GO OK: 0x08000000
```

Selective application erase/write excluded audio page 0x0807F000 and calibration
page 0x0807F800. No post-GO query or movement command. No lifted-wheel or
ground test; readback is not evidence of physical speed or route accuracy.
Previous verified image: 9ba87c3. K210 remains 20e8c72.
