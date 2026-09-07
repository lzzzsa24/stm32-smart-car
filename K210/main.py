# -*- coding: utf-8 -*-
"""
K210 循迹（曲线识别）+ 障碍检测 —— 串口上报版 v4
================================================
K210 只当传感器：跟踪窗口内选中整条黑线，报它的偏移/方向角/底边位置，
外加障碍（牛皮纸箱）三边，打包发 STM32，主控做所有决策。

串口协议：
    $<status>,<off>,<angle>,<bottom>,<obs>,<bot>,<left>,<right>#

接线：K210 8脚(TX)->STM32 PD6(RX)，6脚(RX)->STM32 PD5(TX)，共地，
115200 8-N-1。BOOT 键短按切换屏幕叠加显示，默认关闭。
"""

import gc
import math
import lcd
import sensor
import time
from board import board_info
from maix import GPIO
from machine import UART
from fpioa_manager import fm

SENSOR_VFLIP = 0

BLACK_THRESHOLD = (0, 26, -30, 30, -30, 7)
CENTER_X = 160
MIN_W = 4
MIN_H = 12
PIXELS_THRESHOLD = 150
AREA_THRESHOLD = 150
MERGE = True
MARGIN = 10

BAND_HALF_W = 80
ALPHA = 0.6

SIZE_REF = 2000
SIZE_WEIGHT = 0.5
BOTTOM_WEIGHT = 0.4

LOST_HOLD_MS = 400

OBSTACLE_ROI_TOP = 40
OBSTACLE_ROI = (0, OBSTACLE_ROI_TOP, 320, 240 - OBSTACLE_ROI_TOP)
OBSTACLE_THRESHOLDS = [(15, 75, -128, 127, 7, 127)]
MIN_OBSTACLE_W = 15
OBSTACLE_PIXELS = 200

PRINT_EVERY_N = 5
GC_EVERY_N = 10
SEND_INTERVAL_MS = 50
AXIS_LEN = 40

fm.register(8, fm.fpioa.UART1_TX, force=True)
fm.register(6, fm.fpioa.UART1_RX, force=True)
uart = UART(UART.UART1, 115200, 8, 0, 1,
            timeout=1000, read_buf_len=4096)

lcd.init(freq=15000000)
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)
sensor.set_vflip(SENSOR_VFLIP)
sensor.skip_frames(time=500)
sensor.set_auto_gain(False)
sensor.set_auto_whitebal(False)
clock = time.clock()
last_send = time.ticks_ms()

fm.register(board_info.BOOT_KEY, fm.fpioa.GPIOHS0)
boot_btn = GPIO(GPIO.GPIOHS0, GPIO.IN)
show_overlay = False
boot_pressed = False

frame_no = 0
anchor_x = None
last_angle = 90
lost_since = None


def pick_best(cands):
    """离中线近、面积大且靠下的候选优先。"""
    best = None
    best_score = None
    for blob in cands:
        distance = abs(blob.cx() - CENTER_X) / 160.0
        size = min(blob.pixels(), SIZE_REF) / float(SIZE_REF)
        bottom_score = (blob.y() + blob.h()) / 240.0
        score = (size - SIZE_WEIGHT * distance +
                 BOTTOM_WEIGHT * bottom_score)
        if best is None or score > best_score:
            best = blob
            best_score = score
    return best


def filter_wide(blobs, min_width):
    return [blob for blob in blobs if blob.w() >= min_width]


while True:
    frame_no += 1
    if frame_no % GC_EVERY_N == 0:
        gc.collect()
    clock.tick()
    img = sensor.snapshot()

    boot_now = (boot_btn.value() == 0)
    if boot_now and not boot_pressed:
        show_overlay = not show_overlay
        print("show_overlay =", show_overlay)
    boot_pressed = boot_now

    if anchor_x is None:
        anchor_x = CENTER_X

    x0 = anchor_x - BAND_HALF_W
    if x0 < 0:
        x0 = 0
    x1 = anchor_x + BAND_HALF_W
    if x1 > 320:
        x1 = 320
    roi = (x0, 0, x1 - x0, 240)

    blobs = img.find_blobs([BLACK_THRESHOLD], roi=roi,
                           pixels_threshold=PIXELS_THRESHOLD,
                           area_threshold=AREA_THRESHOLD,
                           merge=MERGE, margin=MARGIN)
    candidates = filter_wide(blobs, MIN_W)

    if not candidates:
        blobs = img.find_blobs([BLACK_THRESHOLD],
                               pixels_threshold=PIXELS_THRESHOLD,
                               area_threshold=AREA_THRESHOLD,
                               merge=MERGE, margin=MARGIN)
        candidates = filter_wide(blobs, MIN_W)
        if candidates:
            selected = pick_best(candidates)
            anchor_x = selected.cx()
        else:
            selected = None
    else:
        selected = pick_best(candidates)
        anchor_x = int(ALPHA * selected.cx() +
                       (1.0 - ALPHA) * anchor_x)

    obstacle_flag = 0
    obstacle_bottom = 0
    obstacle_left = 0
    obstacle_right = 0
    obstacle = None
    blobs = img.find_blobs(OBSTACLE_THRESHOLDS, roi=OBSTACLE_ROI,
                           pixels_threshold=OBSTACLE_PIXELS,
                           area_threshold=OBSTACLE_PIXELS,
                           merge=MERGE, margin=MARGIN)
    obstacle_candidates = filter_wide(blobs, MIN_OBSTACLE_W)
    if obstacle_candidates:
        obstacle = max(obstacle_candidates, key=lambda blob: blob.pixels())
        x, y, w, h = obstacle.rect()
        obstacle_flag = 1
        obstacle_left = x
        obstacle_right = x + w
        obstacle_bottom = y + h

    angle = None
    offset = None
    if selected is not None:
        angle = selected.rotation() * 57.2958
        if angle > 180:
            angle -= 180
        last_angle = int(angle)
        lost_since = None

        status = 1
        offset = selected.cx() - CENTER_X
        bottom = selected.x() + selected.w() // 2
    else:
        status = 0
        offset = -1
        bottom = -1
        now = time.ticks_ms()
        if lost_since is None:
            lost_since = now
        if time.ticks_diff(now, lost_since) < LOST_HOLD_MS:
            angle = last_angle
        else:
            angle = -1

    now = time.ticks_ms()
    if time.ticks_diff(now, last_send) >= SEND_INTERVAL_MS:
        last_send = now
        uart.write("$%d,%d,%d,%d,%d,%d,%d,%d#" %
                   (status, offset,
                    angle if angle is not None else -1, bottom,
                    obstacle_flag, obstacle_bottom,
                    obstacle_left, obstacle_right))

    if show_overlay:
        if obstacle is not None:
            img.draw_rectangle(obstacle.rect(), color=(255, 0, 0),
                               thickness=2)

        window_x0 = anchor_x - BAND_HALF_W
        if window_x0 < 0:
            window_x0 = 0
        window_x1 = anchor_x + BAND_HALF_W
        if window_x1 > 320:
            window_x1 = 320
        img.draw_rectangle((window_x0, 0, window_x1 - window_x0, 240),
                           color=(0, 255, 255), thickness=1)

        if selected is not None:
            center_x = selected.cx()
            center_y = selected.cy()
            radians = selected.rotation()
            img.draw_rectangle(selected.rect(), color=(0, 255, 0),
                               thickness=2)
            img.draw_cross(center_x, center_y, color=(255, 0, 0), size=10)
            dx = math.cos(radians) * AXIS_LEN
            dy = math.sin(radians) * AXIS_LEN
            img.draw_line(int(center_x - dx), int(center_y - dy),
                          int(center_x + dx), int(center_y + dy),
                          color=(255, 255, 0), thickness=2)

        img.draw_string(0, 0, "FPS:%2.1f" % clock.fps(),
                        color=(255, 255, 255), scale=1)
        img.draw_string(0, 12, "s%d off%+d ang%s bot%d" %
                        (status, offset if offset is not None else -1,
                         ("%d" % angle) if angle is not None else "-1",
                         bottom), color=(255, 255, 255), scale=1)
        img.draw_string(0, 26, "obs%d bot%d L%d R%d" %
                        (obstacle_flag, obstacle_bottom,
                         obstacle_left, obstacle_right),
                        color=(255, 255, 255), scale=1)

    lcd.display(img)

    if frame_no % PRINT_EVERY_N == 0:
        print("s=%d off=%s ang=%s bot=%s | obs=%d bot=%d L=%d R=%d fps=%2.1f" %
              (status,
               str(offset) if offset is not None else "-1",
               str(angle) if angle is not None else "-1",
               str(bottom), obstacle_flag, obstacle_bottom,
               obstacle_left, obstacle_right, clock.fps()))
