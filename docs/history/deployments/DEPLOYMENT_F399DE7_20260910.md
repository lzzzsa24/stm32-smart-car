# Comprehensive deployment f399de7

## Repeat deployment after USB reconnect

User requested a repeat of this same source. Two checks found Bluetooth ports
only and did not open the programmer. On the latest retry CH340K COM11 returned.
Clean source and same120008-byte BIN SHA256 below verified before programming.
Session34071: BOOTLOADER ACK, ERASE OK59 pages, WRITE OK120008,
VERIFY OK120008, GO OK0x08000000; exit0. Reserved pages excluded.
No K210 access, post-GO query or physical test.

- Source f399de7138e6538e943709600cef505be6fa596b merges8fdca11 onto7936efd; includes edb7c1b angle-start exit.
- Mode3 confirmed horn sign plays one phrase; mode4 retains five.
- Clean source, full sign suite, diff check and ARM build passed.
- BIN120008; SHA256 DEA054353CE2773F55B44CCE11C6D9318311002A1E7A6EFF7C70E1D4B4DAEA2B.
- HEX SHA256 10B1E3184877FE06828146E03F127F6ED8A459B9969A43144B10C39CD08DE972.
- Artifacts: F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
- Fresh CH340K COM11; programmer57600 PreserveLastPage: BOOTLOADER ACK, ERASE OK59 pages,
  WRITE OK120008, VERIFY OK120008, GO OK0x08000000; exit0.
- Audio/calibration pages excluded. K210/main code unchanged. No post-GO query, physical test or push.
- Rollback source: rollback/2026-09-10-before-8fdca11 ->7936efd; previous board source ec15dda.
