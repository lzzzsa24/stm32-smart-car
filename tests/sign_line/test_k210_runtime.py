"""Execute the real K210 main loop with fake devices; no serial/hardware access."""
import ast
import sys
from pathlib import Path
from types import SimpleNamespace as NS
from unittest.mock import patch

SOURCE = Path(__file__).resolve().parents[2] / 'K210/sign_mode34.py'
code = SOURCE.read_text(encoding='utf-8')
PERIOD = 1 << 30

def diff(a, b):
    return ((a - b + PERIOD // 2) % PERIOD) - PERIOD // 2

# Extract actual helpers without executing hardware initialization.
tree = ast.parse(code)
helpers = [n for n in tree.body if isinstance(n, (ast.FunctionDef, ast.ClassDef))]
scope = {'time': NS(ticks_diff=diff), 'THRESHOLD': .2, 'BOOT_DEBOUNCE_MS': 30}
scope.update(ARROW_SCORE_MARGIN=.15, ARROW_OVERLAP_MIN=.35)
exec(compile(ast.Module(body=helpers, type_ignores=[]), str(SOURCE), 'exec'), scope)
pick, frame = scope['select_route_detection'], scope['detection_frame']
left = (10, 10, 40, 40, 0, .25)
right = (20, 20, 40, 40, 1, .95)
horn = (20, 20, 40, 40, 2, .99)
assert pick([horn, left]) == left
assert pick([left, right]) == right
assert pick([right, left]) == right
assert pick([left, (10,10,40,40,1,.30)]) is None
assert pick([left, (200,10,40,40,1,.95)]) is None
assert pick([horn, (20,20,40,40,0,.95), (20,20,40,40,1,.21)])[4] == 0
assert pick([horn]) is None
assert pick([(0, 0, 1, 1, 1, .19), left]) == left
assert pick([(0, 0, 1, 1, 1, float('nan')), left]) == left
assert pick([(400, 0, 20, 20, 1, .99), left]) == left
assert frame(None) == '$D,-1,0,0,0#\n'
assert frame(left) == '$D,0,25,30,30#\n'
assert frame((310, 230, 40, 40, 1, .5)) == '$D,1,50,315,235#\n'
assert frame((-20, -20, 40, 40, 0, .5)) == '$D,0,50,10,10#\n'
button = scope['BootToggle'](1, PERIOD-20)
assert not button.update(0, PERIOD-10)
assert not button.update(1, PERIOD-5)  # bounce
assert not button.update(0, 0)
assert not button.update(0, 29)
assert button.update(0, 30)
assert not button.update(0, 1000)  # long hold
assert not button.update(1, 1100)
assert not button.update(1, 1130)
assert not button.update(0, 1200)
assert button.update(0, 1230)
held = scope['BootToggle'](0, 0)
assert not held.update(0, 1000)  # boot held at initialization

class EndSimulation(Exception):
    pass

def run_loop(button_enabled, low_memory=False, detections=None):
    detections = [left, horn] if detections is None else detections
    expected = frame(pick(detections))
    now, images, tx, events, logs, collections = [PERIOD-300], [], [], [], [], []
    class Image:
        def __init__(self): self.draws = 0; self.text = []
        def draw_rectangle(self, *args, **kwargs): self.draws += 1
        def draw_string(self, *args, **kwargs): self.draws += 1; self.text.append(args[2])
    class Clock:
        def tick(self): now[0] += 50
        def fps(self): return 20.0
    def snapshot():
        if len(images) == 24: raise EndSimulation()
        image = Image(); images.append(image); return image
    class KPU:
        def load_kmodel(self, path):
            assert path == '/sd/KPU/road_sign_det/road_sign_det.kmodel'
        def init_yolo2(self, anchors, **kwargs):
            assert kwargs['threshold'] == .2 and kwargs['classes'] == 5
        def run_with_output(self, image): events.append(('infer', len(images)))
        def regionlayer_yolo2(self): return detections
    class UART:
        UART1 = 1
        def __init__(self, *args, **kwargs): pass
        def write(self, data):
            tx.append((now[0], data)); events.append(('tx', len(images))); return len(data)
    class GPIO:
        GPIOHS0, IN = 0, 0
        def __init__(self, *args): pass
        def value(self): return 0 if button_enabled and 5 <= len(images) <= 15 else 1
    modules = {
        'time': NS(ticks_ms=lambda: now[0] % PERIOD, ticks_diff=diff,
                   ticks_add=lambda t,d: (t+d) % PERIOD, clock=Clock),
        'gc': NS(collect=lambda: collections.append(now[0]),
                 mem_free=lambda: 32000 if low_memory else 256000),
        'sensor': NS(RGB565=1, QVGA=2, reset=lambda: None,
                     set_pixformat=lambda x: None, set_framesize=lambda x: None,
                     set_vflip=lambda x: None, set_hmirror=lambda x: None,
                     skip_frames=lambda **k: None, snapshot=snapshot),
        'lcd': NS(RED=1, init=lambda **k: None, clear=lambda x: None,
                  display=lambda img: events.append(('lcd', len(images)))),
        'maix': NS(KPU=KPU, GPIO=GPIO), 'machine': NS(UART=UART),
        'board': NS(board_info=NS(BOOT_KEY=16)),
        'fpioa_manager': NS(fm=NS(register=lambda *a,**k: None,
                                  fpioa=NS(UART1_TX=1, UART1_RX=2, GPIOHS0=0)))
    }
    namespace = {'print': lambda *a: logs.append(a)}
    with patch.dict(sys.modules, modules):
        try: exec(compile(code, str(SOURCE), 'exec'), namespace)
        except EndSimulation: pass
    assert len([e for e in events if e[0]=='infer']) == 24
    assert 10 <= len(tx) <= 12
    assert all(b[0]-a[0] >= 100 for a,b in zip(tx,tx[1:]))
    assert all(v == expected for _,v in tx)
    for kind, index in events:
        if kind == 'lcd' and ('tx', index) in events:
            assert events.index(('tx', index)) < events.index(('lcd', index))
    assert len(logs) == 3  # initialization only; no per-frame debug printing
    assert namespace['SHOW_BOXES'] == button_enabled  # held button toggles once
    displayed = [img for kind,index in events if kind=='lcd' for img in [images[index-1]]]
    assert all(any(t.startswith('TX:') for t in img.text) for img in displayed)
    assert all(img.draws == 1 for img in displayed) if not button_enabled else True
    chosen = pick(detections)
    text = scope['transmit_label'](chosen, detections)
    assert displayed[-1].text[-1] == text
    assert len(collections) >= 24 if low_memory else 4 <= len(collections) < 12
    return tx

assert run_loop(False) == run_loop(True)  # overlay changes must not change wire output
run_loop(False, low_memory=True)
run_loop(False, detections=[left,right])
run_loop(True, detections=[left,(10,10,40,40,1,.30)])
run_loop(False, detections=[horn])
assert scope['transmit_label'](None, []) == 'TX:NONE EMPTY'
assert scope['transmit_label'](None, [horn]) == 'TX:NONE NONARROW'
assert scope['transmit_label'](None, [left, (10,10,40,40,1,.30)]) == 'TX:NONE CONFLICT'
print('PASS: K210 helpers, BOOT bounce/hold/wrap, real loop UART before display, overlay isolation, GC fallback')
