# -*- coding: utf-8 -*-
"""
K210 路标识别（目标检测模型）—— 5 类蓝底圆形路标
================================================
模型：det.kmodel（road_sign_detect 训练产物，YOLO2 检测）
五类：0=left左转 1=right右转 2=horn鸣笛 3=one数字1 4=two数字2

部署前准备：
  1. 把 det.kmodel 复制到 SD 卡，路径见下方 KMODEL_PATH；
  2. 脚本存为 main.py（或 IDE 直接运行）。

首阶段：默认显示原始画面，短按 BOOT 切换五类检测框；串口仅发送 left/right。
同帧出现两个方向视为歧义，发送无候选，等待重新确认。
串口输出（发给主控 STM32，8=TX 6=RX 115200）：
  $D,<class>,<score>,<cx>,<cy>#
    class: 本阶段仅 0左半弧 / 1右半弧；其他类不发动作候选
    score: 置信度 0~100（百分制）
    cx,cy: 检测框中心坐标（320x240 画面内）
  未检测到路标：$D,-1,0,0,0#
"""

import gc
import time
import sensor
import lcd
from maix import KPU
from maix import GPIO
from machine import UART
from board import board_info
from fpioa_manager import fm

# ---------------- 参数 ----------------
SENSOR_VFLIP   = 0          # 延续此前针对实机倒置画面的修改
SENSOR_HMIRROR = 0          # 不镜像，左右标签需以实物箭头复核

KMODEL_PATH    = "/sd/KPU/road_sign_det/road_sign_det.kmodel"

LABELS         = ["left", "right", "horn", "one", "two"]   # 顺序必须和 label.txt 一致
ANCHOR         = (1.69, 2.28, 2.75, 4.22, 3.91, 4.02,
                  4.69, 4.66, 4.69, 6.09)                  # anchor.txt 第二行，5 个框
THRESHOLD      = 0.2          # 2026-09-07 用户指定；动作确认由 STM32 多帧判定
NMS_VALUE      = 0.3          # 非极大值抑制，一般不用改
ARROW_SCORE_MARGIN = 0.15     # 同一目标的左右冲突需有明显分差
ARROW_OVERLAP_MIN = 0.35
SEND_INTERVAL  = 100          # 串口发送间隔 ms
SHOW_BOXES     = False        # 参考 v2.0：短按 BOOT 切换框/标签/FPS
DISPLAY_INTERVAL = 100        # LCD 最多 10 fps；不限制 KPU 推理循环
BOOT_DEBOUNCE_MS = 30
DEBUG_PRINT    = False        # 避免每 100 ms 打印拖慢串口和推理
DEBUG_INTERVAL = 500
GC_INTERVAL    = 200          # 定期回收；低内存时提前回收
GC_LOW_BYTES   = 64 * 1024


class BootToggle:
    def __init__(self, initial, now):
        self.stable = initial
        self.candidate = initial
        self.since = now

    def update(self, value, now):
        if value != self.candidate:
            self.candidate = value
            self.since = now
        elif value != self.stable and time.ticks_diff(now, self.since) >= BOOT_DEBOUNCE_MS:
            self.stable = value
            return value == 0  # 按下只切换一次；长按不重复
        return False


def valid_detection(item):
    return (len(item) >= 6 and item[4] in (0, 1, 2, 3, 4)
            and THRESHOLD <= item[5] <= 1.0
            and item[2] > 0 and item[3] > 0
            and item[0] < 320 and item[1] < 240
            and item[0] + item[2] > 0 and item[1] + item[3] > 0)


def detection_frame(best):
    if best is None:
        return "$D,-1,0,0,0#\n"
    # 框可能部分超出图像；发送可见区域中心，满足 STM32 严格范围检查。
    x0, y0 = max(0, best[0]), max(0, best[1])
    x1, y1 = min(320, best[0] + best[2]), min(240, best[1] + best[3])
    cx = min(319, int((x0 + x1) // 2))
    cy = min(239, int((y0 + y1) // 2))
    return "$D,%d,%d,%d,%d#\n" % (best[4], int(best[5] * 100), cx, cy)

def arrow_overlap(a, b):
    w = max(0, min(a[0]+a[2], b[0]+b[2])-max(a[0], b[0]))
    h = max(0, min(a[1]+a[3], b[1]+b[3])-max(a[1], b[1]))
    intersection = w*h
    return intersection / (a[2]*a[3] + b[2]*b[3] - intersection)


def select_route_detection(detections):
    """同一框的低分反向误检不否决高分箭头；分离标志/近分冲突仍不猜。"""
    arrows = [item for item in detections
              if valid_detection(item) and item[4] in (0, 1)]
    best = None
    for item in arrows:
        if best is None or item[5] > best[5]:
            best = item
    if best is not None:
        for item in arrows:
            if item[4] != best[4] and (
                    best[5] - item[5] < ARROW_SCORE_MARGIN or
                    arrow_overlap(best, item) < ARROW_OVERLAP_MIN):
                return None
    return best


# ---------------- 初始化 ----------------
lcd.init(freq=15000000)
lcd.clear(lcd.RED)

sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)          # 320x240，和模型输入一致
sensor.set_vflip(SENSOR_VFLIP)
sensor.set_hmirror(SENSOR_HMIRROR)
sensor.skip_frames(time=500)
clock = time.clock()

# UART1：8=TX 6=RX（和参考 usart_connect.py 一致）
fm.register(8, fm.fpioa.UART1_TX, force=True)
fm.register(6, fm.fpioa.UART1_RX, force=True)
uart = UART(UART.UART1, 115200, 8, 0, 1, timeout=1000, read_buf_len=4096)

fm.register(board_info.BOOT_KEY, fm.fpioa.GPIOHS0)
boot = GPIO(GPIO.GPIOHS0, GPIO.IN)

# 加载模型
print("loading model ...")
kpu = KPU()
kpu.load_kmodel(KMODEL_PATH)
kpu.init_yolo2(ANCHOR,
               anchor_num=len(ANCHOR) // 2,
               img_w=320, img_h=240,
               net_w=320, net_h=240,
               layer_w=10, layer_h=8,
               threshold=THRESHOLD,
               nms_value=NMS_VALUE,
               classes=len(LABELS))
print("SIGN34 ready; model=%s threshold=%.2f; vflip=%d hmirror=%d" %
      (KMODEL_PATH, THRESHOLD, SENSOR_VFLIP, SENSOR_HMIRROR))
print("SIGN34 v2-display; BOOT toggles boxes; UART arrows only")

last_tx_text = "TX:WAIT"
last_gc = time.ticks_ms()
last_send = time.ticks_add(last_gc, -SEND_INTERVAL)
last_display = time.ticks_add(last_gc, -DISPLAY_INTERVAL)
last_debug = time.ticks_add(last_gc, -DEBUG_INTERVAL)
boot_toggle = BootToggle(boot.value(), last_gc)

# ---------------- 主循环 ----------------
while True:
    now = time.ticks_ms()
    if time.ticks_diff(now, last_gc) >= GC_INTERVAL or gc.mem_free() < GC_LOW_BYTES:
        gc.collect()
        last_gc = time.ticks_ms()
    clock.tick()
    if boot_toggle.update(boot.value(), time.ticks_ms()):
        SHOW_BOXES = not SHOW_BOXES
    img = sensor.snapshot()

    kpu.run_with_output(img)
    dect = kpu.regionlayer_yolo2() or ()
    best = select_route_detection(dect)

    # 当前帧结果优先发给主控，绘图/LCD 放在后面，不重发缓存识别结果。
    now = time.ticks_ms()
    if time.ticks_diff(now, last_send) >= SEND_INTERVAL:
        frame = detection_frame(best)
        uart.write(frame)
        last_tx_text = "TX:NONE" if best is None else "TX:%s %d" % (
            "L" if best[4] == 0 else "R", int(best[5]*100))
        last_send = now
        if DEBUG_PRINT and time.ticks_diff(now, last_debug) >= DEBUG_INTERVAL:
            print(frame.strip())
            last_debug = now

    if time.ticks_diff(now, last_display) >= DISPLAY_INTERVAL:
        if SHOW_BOXES:
            for item in dect:
                if not valid_detection(item):
                    continue
                x, y, w, h, cls, score = item[:6]
                img.draw_rectangle(x, y, w, h, color=(0, 255, 0))
                img.draw_string(max(0, x), max(0, y), "%s %.2f" % (LABELS[cls], score),
                                color=(255, 0, 0), scale=2.0)
            img.draw_string(0, 0, "%2.1ffps" % clock.fps(), color=(0, 60, 255), scale=2.0)
        img.draw_string(0, 220, last_tx_text, color=(255, 255, 0), scale=1.0)
        lcd.display(img)
        last_display = time.ticks_ms()
    # 不让上一帧的图像/检测列表拖到下一次 sensor.snapshot() 后才释放。
    del img, dect, best

# kpu.deinit()  # 主循环不会到这里，仅示意
