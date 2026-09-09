# STM32 comprehensive V8 deployment — 2026-09-09

- Requested and deployed source: `c767baa158a25cbf411aae6c1dadb5b631a2e51e`.
- Source worktree: `F:/myproject/jidian/worktrees/comprehensive-v8-auto-recovery`.
- Temporary test branch: `test/comprehensive-v8-auto-recovery-20260908`; not merged into main.
- Source worktree clean; formal build rerun for this exact HEAD.
- ARM text/data/bss: 99392/64/11976 bytes. BIN: 99460 bytes.
- BIN SHA-256: `7268C24CD4B0C8534AD509E477495EAE6CAAB5C453DFD869ABC89FA028EE382B`.
- HEX SHA-256: `F661FB276C15B97D4186C9A2D7E47192BC4E7EBA7FA6C1E2B9CD284FD04994C7`.
- Artifacts: `manual-build-unified-motion/exp7_unified_motion.bin` and `.hex` in the source worktree.
- Enumerated device: USB-SERIAL CH340K (COM11), USB VID_1A86/PID_7522.
- Programmer: `F:/myproject/jidian/tools/stm32_uart_flash.ps1`, PowerShell 7, 57600 baud, `-PreserveLastPage`.
- Selective erase: application pages 0..48 (49 pages); neither audio page `0x0807F000` nor calibration page `0x0807F800` is included. No mass erase.

Observed programmer output:

```text
BOOTLOADER ACK: boot=DTR value=True reset-active=False
ERASE OK: 49 firmware pages; calibration page preserved
WRITE OK: 99460 bytes
VERIFY OK: 99460 bytes
GO OK: 0x08000000
```

Full application readback passed. Reserved-page preservation is established by
the selective erase/write bounds, not a separate data-page readback comparison.
No post-GO STOP/zero-output query was performed, per the user's standing request.
No mode start, lifted-wheel test or ground test was performed. K210 was untouched.
Prior programmer-verified source in shared state was `4589a25`; this record does
not infer any intervening user-programmed image. Source defaults to STOP.
