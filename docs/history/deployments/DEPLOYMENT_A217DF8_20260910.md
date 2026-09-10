# Comprehensive deployment a217df8

- Source a217df81e807f54a7b95fc8c8541876cdc59bfc8 includes7c308e0.
- Fresh CH340K COM11 after reconnect; clean source and tested artifact checked.
- BIN121028; SHA256 308736418B9C96F2BC12B4AB7A06BADD4B1A63584781B6900B01CE120C958079.
- HEX SHA256 2D4C8181B4A53832F5779DDE9C94A7B41DCAE6BC6ED2B019403559B5BE1A76FA.
- Artifacts: F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
- ROM programmer57600 PreserveLastPage: ACK, ERASE OK60 pages, WRITE OK121028,
  VERIFY OK121028, GO OK0x08000000; exit0. Earlier sync failure superseded.
- Audio/calibration pages excluded; K210/main code unchanged. No post-GO query,
  physical test or push. Rollback before-7c308e0 ->6da3ac8.
