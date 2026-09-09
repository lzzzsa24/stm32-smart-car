# Combined speed/handoff deployment

Source 3a00fa8022a2bf991bbbc3fac878df5e62ebcc79; includes 93b408b and
bf3e8e1. Clean worktree and BIN hash checked before flash. Build/test evidence
in docs/history/candidates/COMPREHENSIVE_3A00FA8_20260909.md.
BIN 106384 bytes, SHA256 4E83C0EB990FEF6E44B56F368B2CA8D5A16A5DE25DB586E97523588D692BB36F.
HEX SHA256 A7E7266BC2234AEB5F81E165DB4B41F4BB6CA4B06660BC7C58B4ABC37BF82233.

COM11 freshly enumerated, 57600 baud, PreserveLastPage. Bootloader ACK,
52-page selective erase, WRITE OK 106384 bytes, VERIFY OK 106384 bytes,
GO OK 0x08000000. Audio/calibration pages outside erase/write bounds.
No post-GO query, wheel or ground test. K210 unchanged at 20e8c72 threshold .15.
No main firmware promotion or GitHub push.
