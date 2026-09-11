# -*- coding: utf-8 -*-
"""BOOT 按键：短按切换叠加层显示/隐藏，长按循环切换模式。"""

import time
from board import board_info
from maix import GPIO
from fpioa_manager import fm

DEBOUNCE_MS   = 20      # 去抖时间，防止按键抖动多次触发
LONG_PRESS_MS = 1000    # 长按判定时间（按住超过该时长算长按）


class BootButton:
    """带去抖的 BOOT 键（低电平有效）。

    event() 每次按键返回一个事件：
      'short' —— 短按（松开时未达到长按阈值）
      'long'  —— 长按（按住超过 LONG_PRESS_MS，只触发一次）
      None    —— 无事件
    """

    def __init__(self):
        fm.register(board_info.BOOT_KEY, fm.fpioa.GPIOHS0)
        self.gpio = GPIO(GPIO.GPIOHS0, GPIO.IN)
        self.stable = self.gpio.value()
        self.candidate = self.stable
        self.since = time.ticks_ms()
        self.press_start = None      # 按下时刻
        self.long_fired = False      # 本次按住是否已触发过长按

    def event(self):
        now = time.ticks_ms()
        value = self.gpio.value()

        # 去抖：电平稳定 DEBOUNCE_MS 后才认
        if value != self.candidate:
            self.candidate = value
            self.since = now
        elif value != self.stable and \
                time.ticks_diff(now, self.since) >= DEBOUNCE_MS:
            self.stable = value
            if value == 0:                 # 按下
                self.press_start = now
                self.long_fired = False
            else:                          # 松开
                was_long = self.long_fired
                self.press_start = None
                self.long_fired = False
                if not was_long:
                    return 'short'

        # 长按：按住超过阈值，只触发一次
        if self.stable == 0 and not self.long_fired and \
                self.press_start is not None and \
                time.ticks_diff(now, self.press_start) >= LONG_PRESS_MS:
            self.long_fired = True
            return 'long'

        return None
