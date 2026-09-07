from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parents[2]
main = (ROOT / "Core/Src/main.c").read_text(encoding="utf-8")
irq = (ROOT / "Core/Src/stm32f1xx_it.c").read_text(encoding="utf-8")
k210 = (ROOT / "K210/sign_mode34.py").read_text(encoding="utf-8")
manifest = json.loads((ROOT / "K210/model-manifest.json").read_text(encoding="utf-8"))
model = ROOT / "K210/road_sign_det_20260906.kmodel"

assert "return APP_MODE_SIGN_LINE_ADVANCED;" in main
assert "return APP_MODE_SIGN_LINE_SIMPLE;" in main
assert "Figure8Encoder_Start();" not in main
assert "SquareEncoder_Start();" not in main
assert "sign_line_task(app_mode);" in main
assert "void USART2_IRQHandler(void)" in irq
assert "vision_uart_irq_handler();" in irq
assert "THRESHOLD      = 0.2" in k210
assert 'KMODEL_PATH    = "/sd/KPU/road_sign_det/road_sign_det.kmodel"' in k210
assert 'print("SIGN34 ready;' in k210
assert model.stat().st_size == manifest["bytes"]
assert hashlib.sha256(model.read_bytes()).hexdigest() == manifest["sha256"]
compile(k210, str(ROOT / "K210/sign_mode34.py"), "exec")
print("PASS: mode 3/4 bindings, USART2 IRQ, preserved sign script and model identity")
