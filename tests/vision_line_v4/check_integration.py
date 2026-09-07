from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
main = (ROOT / "Core/Src/main.c").read_text(encoding="utf-8")
ir_source = (ROOT / "Core/Src/ir_remote.c").read_text(encoding="utf-8")
vision_uart = (ROOT / "Core/Src/vision_uart.c").read_text(encoding="utf-8")
k210 = (ROOT / "K210/main.py").read_text(encoding="utf-8")
sign = (ROOT / "K210/sign_mode34.py").read_text(encoding="utf-8")

assert "APP_MODE_VISION_LINE_V4" in main
assert "return APP_MODE_VISION_LINE_V4;" in main
assert "case '5': return IR_REMOTE_VIRTUAL_KEY5;" in main
assert "vision_line_v4_task();" in main
assert "VisionLineV4Control_Step" in main
assert '"VLINE5 CURVE V4 START\\r\\n"' in main
assert "#define IR_COMMAND_NUMBER_5          0x15U" in ir_source
assert "return IR_REMOTE_VIRTUAL_KEY5;" in ir_source
assert "VisionDetectionParser_Consume" in vision_uart
assert "VisionLineV4Parser_Consume" in vision_uart
assert "line_v4_result == VISION_LINE_V4_PARSE_BAD_FRAME" in vision_uart

assert "sensor.set_pixformat(sensor.RGB565)" in k210
assert "show_overlay = False" in k210
assert "SEND_INTERVAL_MS = 50" in k210
assert 'uart.write("$%d,%d,%d,%d,%d,%d,%d,%d#"' in k210
assert "KPU" not in k210
assert "road_sign" not in k210
assert "SIGN34 ready" in sign
compile(k210, str(ROOT / "K210/main.py"), "exec")

print("PASS: mode 5 binding, K210 v4 protocol, no sign model in mode 5")
