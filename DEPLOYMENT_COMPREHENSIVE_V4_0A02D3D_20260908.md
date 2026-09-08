# Comprehensive STM32/K210 test deployment: `0a02d3d`

Date: 2026-09-08 14:18 +08:00

## Source and integrated scope

- Exact STM32 test source:
  `0a02d3d0533fd93df468d91624acbbf245548714`.
- Test branch: `test/comprehensive-v4-20260908`.
- Previous comprehensive source: `f34e7dd`.
- Requested worker increment:
  `a14aaff858772cf8351edfff2009963fcb152395`, cherry-picked as `0a02d3d`.

The increment makes KEY1 and KEY2 use the same current tracking entry profile
and one-snapshot compute/apply cycle. Both now start with smooth tracking,
100-percent middle steering gain and no blind forward travel before the first
line. KEY1 retains its ultrasonic speed cap and all obstacle/bypass ownership;
KEY2 retains its full forward limit. Modes 3--5 are not changed by this
increment.

The test stack also retains the v3 layers: silent current line recovery,
persistent sole-outer powered correction, ten-second sign-probe hold and
continuous short-distance obstacle-bypass travel. It remains separate from
canonical main.

## Build and host verification

- Complete line-recovery/load/bypass suite: passed at both configured search
  speeds.
- The real-source mode-parity replay matched 3600 KEY1/KEY2 samples, including
  initial white, persistent outer evidence, adjacent pairs, center, gaps, loss,
  full/reduced caps and post-bypass reset/re-entry.
- Source-binding checks confirmed that bypass and ultrasonic early-return gates
  retain ownership before the shared line cycle.
- Sign-line suite, including the ten-second probe hold: passed.
- Vision-line-v4 suite: passed.
- Formal ARM build: passed.
- ELF text/data/bss: `84404 / 64 / 11600` bytes.
- BIN size: `84472` bytes.
- BIN SHA-256:
  `BE44622428EC5BB8A216F190773D16DE0FA153AC92E1EF4E051AB7AD346E0E92`.
- HEX SHA-256:
  `3D61C879E7920D8CF37BBF47547E2E6790E773FF1168CC9E0F0BB8D6BF52833E`.

Cached STM32 artifacts:

- `manual-build-candidate-comprehensive-v4-0a02d3d/exp7_unified_motion.bin`
- `manual-build-candidate-comprehensive-v4-0a02d3d/exp7_unified_motion.hex`

## STM32 deployment evidence

- `pnputil` enumerated USB-SERIAL CH340K as `COM11` immediately before flash.
- An application serial `0` was sent before bootloader entry; the board emitted
  its `DEFAULT STOP` startup banner.
- ROM bootloader acknowledged.
- Selective erase covered 42 application pages and preserved the final
  calibration page.
- All `84472` bytes were written and read back with
  `VERIFY OK: 84472 bytes`.
- Execution completed with `GO OK: 0x08000000`.
- Per the user's standing request, no routine post-GO serial STOP/four-wheel
  zero-output check was performed.

## K210 mode 3/4 deployment evidence

- `pnputil` enumerated the K210 USB-SERIAL CH340 as `COM14`.
- Correct application: repository `K210/sign_mode34.py` deployed as
  `/sd/main.py`; modes 3/4 must not use the mode-5 visual-line script.
- The prior `/sd/main.py` was a different 3891-byte file with SHA-256
  `8292B641DFB430A1B08D41728B582D4590C0D59B831705600229360C8B79BD78`.
  It was backed up before replacement.
- The unchanged `/flash/main.py` was backed up as 289 bytes with SHA-256
  `96981B6363E004D653A5FCE5FDA67691C5364EABB6EE13B30629BBCD4DAFFA24`.
- The existing model
  `/sd/KPU/road_sign_det/road_sign_det.kmodel` already matched SHA-256
  `B472A5C45FBB2060CD794BEC7C972D9F58FB40D7DCA27DFE6545125B8E02B901`,
  so the 571432-byte model was not rewritten.
- The 7256-byte SIGN34 script was written and read back byte-for-byte with
  SHA-256
  `2BCFCC5E08671EE0F0E3BD0712A1DD217A3450BFDBD3C3DDA7EFE8807D38A3D8`.
- Soft reboot reported `model load succeed` and `SIGN34 ready`, with the
  expected model path, threshold 0.20 and camera orientation 0/0.
- Backup and deployment manifest:
  `F:/myproject/jidian/validation/comprehensive-v4-0a02d3d-deploy-20260908/k210-backups/backup-20260908-141636`.

## Boundaries

- No driving mode was started automatically.
- No post-deployment STM32-to-K210 UART link query was run.
- No lifted-wheel or ground-driving test was performed.
- Build, readback and K210 startup prove deployed bytes and startup only, not
  physical line following, sign recognition accuracy or route selection.
- Canonical GitHub main remains `v1.2.0-rc.3`; this v4 composite was not merged
  or pushed.
- The unrelated untracked purchase-guide file in the canonical checkout was
  preserved and was not included in this record commit.

Rollback source tag:
`rollback/2026-09-08-before-comprehensive-v4-a14aaff` -> `f34e7dd`

Deployed source tag:
`deployed/2026-09-08-comprehensive-v4-0a02d3d` -> `0a02d3d`
