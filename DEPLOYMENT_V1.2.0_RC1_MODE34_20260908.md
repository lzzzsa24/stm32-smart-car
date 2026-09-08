# v1.2.0-rc.1 STM32 and K210 mode 3/4 deployment

Date: 2026-09-08

## Requested target

- GitHub release: `v1.2.0-rc.1`
- Release merge commit: `3eb68899dbfed5e1e1dfe9035e40802b8df7a960`
- Functional firmware source: `36551f31a9fab431fc7af1fa0b5a79b7162fa643`
- K210 selection: mode 3/4 SIGN34
- Motion authorization: none; keep the vehicle stopped

## STM32 evidence

Immediately before programming, Windows enumerated USB-SERIAL CH340K as
COM11 and USB-SERIAL CH340 as COM14. A native serial handle that did not change
DTR/RTS sent the application STOP byte. The response showed `DRV M=0 P=0 F=0`
and zero requested, measured and PWM values for M1 through M4.

The formal ARM build passed with text/data/bss 84344/64/11536. The programmed
BIN was 84412 bytes with SHA-256
`5C7F43ACCEE850E30F439121254E1FCA4085CFAAC11C6CE10423463070777CCB`.
PowerShell 7 and `tools/stm32_uart_flash.ps1` entered the STM32 ROM bootloader
on COM11 at 57600 baud, selectively erased 42 firmware pages, preserved the
final 2 KiB page, wrote all 84412 bytes, read them back byte-for-byte with
`VERIFY OK`, and completed `GO OK: 0x08000000`.

An additional read-only ROM session read `0x0807F800..0x0807FFFF`. Its SHA-256
was `D0FF1B294B5288D1AE1421EADF5B2D38A8752B76D472FF30BED9028E25B1C5B8`,
exactly matching both historical verified snapshots. The page is all `0xFF`,
so this car currently has no saved profile in that reserved page; it was not
made blank by this deployment. No erase or write command was used in the
additional session, and the application was resumed with GO.

The first two attempts at this additional check stopped before GO because its
new helper initially misclassified the historically blank page and then changed
an adapter-dependent DTR/RTS state. Both attempts occurred after the successful
formal write/readback and issued no erase or write command. The final check
retained the proven control-line state and completed GO successfully.

## K210 evidence

COM14 identified MicroPython/CanMV Yahboom 2.1.1, GC2145 and mounted `/sd`.
The prior `/sd/main.py` was already the exact requested 7256-byte SIGN34 script,
SHA-256
`2BCFCC5E08671EE0F0E3BD0712A1DD217A3450BFDBD3C3DDA7EFE8807D38A3D8`.
It and the 289-byte `/flash/main.py` were backed up before deployment. The
requested script was still written as `/sd/main.py` and read back byte-for-byte.

The existing 571432-byte model at
`/sd/KPU/road_sign_det/road_sign_det.kmodel` matched SHA-256
`B472A5C45FBB2060CD794BEC7C972D9F58FB40D7DCA27DFE6545125B8E02B901`,
so it was verified on-device and not rewritten. Soft reboot reported
`model load succeed`, `SIGN34 ready`, threshold 0.20 and vflip/hmirror 0/0.

Backup and raw validation output are under
`F:/myproject/jidian/validation/v1.2.0-rc1-mode34-deploy-20260908`, including
K210 backup directory `k210-backups/backup-20260908-092112`.

## Stationary board-link check

With the STM32 repeatedly held in STOP, four `VLINK` queries reported SIGN
counts 102, 110, 118 and 126. This proves K210-to-STM32 delivery and successful
parsing of 24 additional SIGN34 protocol frames. `V4` remained zero, confirming
that the active K210 application is not mode 5.

The aggregate `BAD` field rose from 95 to 119 because it includes detection
queue overflow. In STOP, SignRoute does not consume the eight-slot detection
queue, so each later successfully parsed no-target/detection frame replaces the
oldest item. This does not negate the increasing valid/none `SIGN` count.

Final telemetry showed `DRV M=0 P=0 F=0`, all four wheel targets, measured
speeds and PWM outputs at zero, and battery about 8.38 V. No mode 3/4 start
command was sent.

## Verification boundary

- Computer build/link: passed.
- STM32 flash/readback/GO: passed.
- K210 script/model readback and local startup: passed.
- K210-to-STM32 SIGN frame delivery while STOP: passed.
- Wheels-off-ground motion: not performed.
- Ground driving, route selection and physical sign accuracy: not performed.
