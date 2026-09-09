# SIGN34 transmit selection

User reports mode 3/4 OLED VIS:- 0 while K210 displays a sign. This indicates
a parsed no-target observation, not total link loss; it does not prove the
exact preceding detection set. Latest deployment record: STM32 c767baa and
unchanged 7256-byte SIGN34 K210 script. No hardware was accessed this turn.

Confirmed reproducible issue: previous selector rejected every frame containing
both left and right candidates, even when they overlapped and confidence was
0.95 versus 0.25. Raw overlay displayed the boxes despite sending no target.

New policy: pick the strongest valid arrow only if every opposing candidate
overlaps it with IoU >= 0.35 and trails by >= 0.15 confidence. Otherwise send
the existing no-target frame. Separate physical signs and near ties remain
ambiguous. Non-arrow classes remain non-steering. Threshold stays 0.2; model,
orientation, UART pins/baud and frame format remain unchanged.

Always show the last UART-submitted candidate at the bottom of the LCD:
TX:L/R with integer confidence, TX:NONE or initial TX:WAIT. It is a local
transmit indication, not a STM32 acknowledgement. BOOT still toggles raw boxes.
The new evidence lets a test distinguish detected boxes from transmitted route
candidates without changing car control. It does not guarantee recognition.

Python real-loop simulation and integration checks passed: candidate order,
overlap dominance, near tie, separated signs, non-arrow/invalid input, UART
payload versus LCD text, BOOT isolation, scheduling, wrap and low-memory GC.
No STM32 source changed, so no STM32 firmware build/flash is required for this
patch. Deployment source is K210/sign_mode34.py, installed as /sd/main.py only
when explicitly requested; repository K210/main.py remains mode 5.

Base: main 72b0208. Branch: fix/sign-tx-selection. No merge, flash, model
rewrite, serial access or ground test. Confidence/overlap gates need real-scene
validation; persistent TX:NONE can still mean a close conflict or no valid arrow.
