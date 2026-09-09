# Comprehensive bd87633; flash blocked before opening port

Base: 6894af16280d5780b3ba5ddda7b983a529b46425.
Requested c133918cc9547b9505d5608fd28eeb21cd4d690d replayed as 8daf674.
Requested bc40f796ae222175b91176402c4b6f7316193356 replayed as
bd87633eb645344f9dde85ec4b2700ea65e3e5e7.
Branch: test/comprehensive-v15-sign-horn-20260909.
Checkout: F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn.

Fixed bypass is now 360-mm outward and 600-mm parallel. Modes 3/4 ordinary
tracking use KEY2's shared controller with retained recognition slowdown,
route ownership and gyro safeguards. Side obstacle IR remains disabled.
K210 unchanged. Main code unchanged; no GitHub push.

Full sign_line, line_recovery and gyro_turn suites exited 0, including 8000
sample shared-output comparisons and fixed 360/600-mm route simulations.
Formal build exited 0: text/data/bss 108680/64/18336. Source checkout clean;
git diff --check passed. Simulation is not physical validation.

Artifacts under checkout/manual-build-unified-motion:

- exp7_unified_motion.bin: 108748 bytes.
- BIN SHA256: 9171595BC74C4E27310EC337A43E83D653857169C545775BD1E7A30367221116
- exp7_unified_motion.hex SHA256: EB1C11773655FF8BD6C57FDA9F16C5EC85F24B2981D853D31582047AC151430C

## Flash attempt

COM11 appeared in fresh SerialPort enumeration. PowerShell 7 called the normal
stm32_uart_flash.ps1 with 57600 baud and PreserveLastPage, but SerialPort.Open
failed: "连到系统上的设备没有发挥作用。" Exit 1, before bootloader sync,
erase or write. Repeat read-only enumeration still listed COM11; PnP query
was denied access. No repeat flash attempt, motion or K210 access.

User must reconnect STM32 USB. Board Flash remains previous verified 6894af1.
This candidate has NOT passed flash/readback/GO, lifted-wheel or ground test.
