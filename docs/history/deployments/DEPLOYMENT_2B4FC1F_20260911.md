# Four-mode main deployment2b4fc1f

- Source2b4fc1f27b9b405be48ce3e2c6d4f9212f5ff85e includes6594ff0.
- Explicit FOUR-mode main deployment, replacing five-mode62b0de2.
- Fresh CH340K COM11 after replug; exact tested BIN132156 checked.
- BIN SHA256 8C14B8FB10ED8341DD4A0A3FBBE6AC44EF826AA2482624730BCECA7847D872EA.
- HEX SHA256 5E3E41992193398B1444A1939297C41B844BE4974C3EE2178726E09CD5D10C2C.
- Main artifact directory: manual-build-unified-motion.
- ROM programmer57600 PreserveLastPage: ACK, ERASE OK65 pages, WRITE OK132156,
  VERIFY OK132156, GO OK0x08000000; exit0. Reserved audio/calibration excluded.
- K210 unchanged; no post-GO query, wheel/ground test or GitHub push.
- Mode3 exit is unbounded forward until fresh outer capture; no physical validation.
