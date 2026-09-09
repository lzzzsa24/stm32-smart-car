# Comprehensive sign low-speed deployment

ec2dd2f6b9ae20df45f461e45669dc32ad8a77ef replays 11a243f onto 8b91f49.
Includes 24-cm fixed bypass; modes3/4 opt into encoder-accounted low-speed
powered/coast regulation and 500-CPS search cap. Other modes opt out.
Formal build, full sign_line (new low-speed actuator tests), line_recovery
(both speeds) and gyro_turn suites passed. Clean source/diff.
Text/data/bss 109672/64/18344, BIN 109740 bytes.
BIN SHA256 69710DBDEF1E517A297FB8992D761BC5E6552E359763278B94F2552B772D5478.
HEX SHA256 09E17A9C5F1F4AB441013F343A285D17346D928F354EEA44B6753A917D7F5A3C.
Artifacts in worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.

COM11 freshly enumerated, 57600 baud, PreserveLastPage. Bootloader ACK,
54-page selective erase, WRITE OK 109740 bytes, VERIFY OK 109740 bytes,
GO OK 0x08000000. Audio/calibration pages excluded. K210 unchanged at
20e8c72 threshold .15. No post-GO, wheel/ground test, main code promotion or push.
Host synthetic wheel results do not establish physical low-speed performance.
