# Comprehensive gyro exit deployment

Source 6bd0c894203cfd59ebbf967fe77463c47cb74f67 replays only 6bc7d28
on 20e8c72. Confirmed entry ownership and gyro-aligned bounded exit travel;
pause, RGB, horn, K210 and other modes unchanged. Sign suite and formal ARM
build passed; clean source/diff check. Text/data/bss 105660/64/18296.
BIN 105728 bytes, SHA256 221D53F8E7F349D94C15DEF5C6C83189BD05EB92141E6CAEAD7171E3134ECB17.
HEX SHA256 6027B49F07F6174BAFF744E2E47BDB8B08562B20AFF078956E9AFCCF42510B74.
Artifacts: F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.

COM11 enumerated, 57600 baud, PreserveLastPage: bootloader ACK, 52 application
pages erased, WRITE OK 105728 bytes, VERIFY OK 105728 bytes, GO OK 0x08000000.
Audio/calibration pages excluded from erase/write. K210 unchanged, still
paired 20e8c72 threshold .15. No post-GO query, wheel/ground or physical sign
test. No main firmware promotion or GitHub push.
