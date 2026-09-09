# Comprehensive ARC progress deployment

5fdb84aee6c4e82b9d95435d90a072147ac3cb75 replays 0768639 onto a773196.
Includes previously unflashed mode1 fixed rectangle 74c0d66 plus current
sign capture/ARC origin and slower exit alignment updates. Other modes,
K210, horn and pause retained. Full sign_line suite and ARM build passed;
fixed rectangle line/gyro regression passed in preceding a773196 integration.
Text/data/bss 108840/64/18320, BIN 108908 bytes.
BIN SHA256 900720C27D9990E9B19DA4381E0256585CC7BD74FFC3C084672F7F4F9B133751.
HEX SHA256 C2BD4C84BF1A1F963571D7D08259EE0C5267A613377A57C5B064BD3F967F67DD.
Artifacts in worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.

COM11 enumerated, 57600 baud PreserveLastPage: bootloader ACK, selective
54-page erase, WRITE OK 108908 bytes, VERIFY OK 108908 bytes, GO OK 0x08000000.
Audio/calibration pages excluded. No post-GO query, wheel or ground test.
K210 unchanged at 20e8c72 threshold .15. No main code promotion or push.
