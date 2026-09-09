# Comprehensive RGB-off candidate; flash pending

Source: `c50d3c81a992fc3d5cd7e91d42ceeaa9c3385821`.
Worktree: `F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn`.
Merged `6908938` into V15, retaining five-repeat horn `9cea6bc`, STRACE and
gyro/navigation behavior. Modes 3/4 clear each RGB channel on its actual port,
including entry and fault paths. Other motor controls remain unchanged.

Sign-line suite (gyro, bus, route, trace, horn, K210 simulation), formal ARM
build/link and diff check passed. BIN: 104232 bytes.
Artifacts are in that worktree's `manual-build-unified-motion` directory.
BIN SHA256: `2C980CA65E5C267D604AB70806C55B21F478E2998E3A1A1643A73497EBFD0DB8`.
HEX SHA256: `4B3C026C7E18D0670452438C8D4952B0A4E41867030D84F4D94000E3D9974263`.

User requested flashing. Live Win32_SerialPort enumeration returned only
Bluetooth COM3..COM10, no USB STM32 port. No erase, write, readback or GO was
attempted. STM32 remains recorded V14; K210 is unchanged. No physical test.
After reconnect, enumerate again and use the reviewed UART programmer with
selective erase (`-PreserveLastPage`), full readback and GO. Preserve both
audio page 0x0807F000 and calibration page 0x0807F800. Do not use an old BIN.
