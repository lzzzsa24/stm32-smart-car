# -*- coding: utf-8 -*-
"""
K210 路标识别（目标检测模型）—— 5 类蓝底圆形路标
================================================
模型：det.kmodel（road_sign_detect 训练产物，YOLO2 检测）
五类：0=left左转 1=right右转 2=horn鸣笛 3=one数字1 4=two数字2

部署前准备：
  1. 把 det.kmodel 复制到 SD 卡，路径见下方 KMODEL_PATH；
  2. 脚本存为 main.py（或 IDE 直接运行）。

首阶段：屏幕显示五类，串口仅发送用于圆环选弧的 left/right。
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
SEND_INTERVAL  = 100          # 串口发送间隔 ms

def select_route_detection(detections):
    """只在箭头中选最高分；左右同时出现时不猜测路线。"""
    best = None
    direction = None
    for item in detections:
        cls = item[4]
        if cls not in (0, 1):
            continue
        if direction is not None and cls != direction:
            return None
        direction = cls
        if best is None or item[5] > best[5]:
            best = item
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

last_send = 0

# ---------------- 主循环 ----------------
while True:
    gc.collect()
    clock.tick()
    img = sensor.snapshot()

    kpu.run_with_output(img)
    dect = kpu.regionlayer_yolo2()
    fps = clock.fps()

    # 屏幕保留所有类别，便于观察；UART 只给圆环左右选弧候选。
    for item in dect:
        x, y, w, h, cls, score = item[:6]
        img.draw_rectangle(x, y, w, h, color=(0, 255, 0))
        img.draw_string(x, y, "%s %.2f" % (LABELS[cls], score),
                        color=(255, 0, 0), scale=2.0)
    best = select_route_detection(dect)

    if best is not None:
        x, y, w, h, cls, score = best[0], best[1], best[2], best[3], best[4], best[5]
        cx = x + w // 2
        cy = y + h // 2
        name = LABELS[cls]

        # 串口发送（限频）
        now = time.ticks_ms()
        if time.ticks_diff(now, last_send) >= SEND_INTERVAL:
            uart.write("$D,%d,%d,%d,%d#\n" % (cls, int(score * 100), cx, cy))
            last_send = now
            print("sign:", name, "score=%.2f" % score, "cx=%d cy=%d" % (cx, cy))
    else:
        # 无箭头候选，或左右同时出现无法唯一选弧
        now = time.ticks_ms()
        if time.ticks_diff(now, last_send) >= SEND_INTERVAL:
            uart.write("$D,-1,0,0,0#\n")
            last_send = now

    img.draw_string(0, 0, "%2.1ffps" % fps, color=(0, 60, 255), scale=2.0)
    lcd.display(img)

# kpu.deinit()  # 主循环不会到这里，仅示意
