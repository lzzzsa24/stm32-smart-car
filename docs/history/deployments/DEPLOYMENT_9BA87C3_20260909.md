# Comprehensive deployment 9ba87c3

Requested worker: 5bb2abd2ec0b2eb7f37500d55423fa47899b5d9f.
Cherry-picked onto a5b618d, result 9ba87c30c4994d54a51bf13cb27dc63b06e8d21c.
Branch: test/comprehensive-v15-sign-horn-20260909.
Checkout: F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn.

This is the fixed-route isolation test: obstacle IR emitters, triggering,
side-input intervention and status display are disabled. Front ultrasonic,
four-probe tracking, remote STOP, battery ADC and sign low-speed power remain.
Side obstacles outside the ultrasonic beam no longer have IR protection.
Main firmware code and K210 were not changed; no GitHub push was requested.

## Computer verification

Formal build and tests/sign_line/run.cmd, tests/line_recovery/run.cmd,
tests/gyro_turn/run.cmd all exited 0. git diff --check passed and source
checkout was clean. Simulation is not real-world clearance validation.
ELF text/data/bss: 109140/64/18344.

Artifacts under the checkout's manual-build-unified-motion:

- exp7_unified_motion.bin: 109208 bytes.
- BIN SHA256: 1D2E1F50EE24D239336E0285D49BD3B79F603F07CD680842BEB8B7F434E27031
- exp7_unified_motion.hex SHA256: 62A238C1D974F7928BE2A3F89B81899F8C6E0A5ABAFC279D260EF77E26859747

## Deployment evidence

COM11 enumerated immediately before programming. Used PowerShell 7 with
F:/myproject/jidian/tools/stm32_uart_flash.ps1, -BaudRate 57600 and
-PreserveLastPage against the above BIN.

```text
BOOTLOADER ACK: boot=DTR value=True reset-active=False
ERASE OK: 54 firmware pages; calibration page preserved
WRITE OK: 109208 bytes
VERIFY OK: 109208 bytes
GO OK: 0x08000000
```

Exit 0. Selective application erase/write excludes audio page 0x0807F000
and calibration page 0x0807F800. No post-GO STOP/zero-output query or motion
command was sent. No lifted-wheel or ground test was performed.
Previous verified STM32 image: ec2dd2f. K210 remains 20e8c72.
