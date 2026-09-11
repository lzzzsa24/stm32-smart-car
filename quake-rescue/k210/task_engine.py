# -*- coding: utf-8 -*-
"""模式门控引擎：按模式决定识别方式，去抖后选标记，并支持自动切换。

纯逻辑模块，不接触硬件，便于单独阅读和讲解。
"""

import time

import config


class TaskEngine:
    """维护当前模式；把各检测结果（颜色/人）合成候选标记。"""

    def __init__(self):
        self.mode = config.INITIAL_MODE      # 上电初始模式
        self._last_marker = config.MARKER_NONE
        self._hold = 0
        self._cooldown_until = 0             # 自动切换的冷却截止时刻
        self._reset_since = -1               # 连续识别到复位牌(蓝)的起始时刻；-1=未在识别
        self._task_marker = config.MARKER_NONE   # 连续识别的任务牌(1/2)
        self._task_since = -1                     # 连续识别任务牌的起始时刻；-1=未在识别

    def set_mode(self, mode):
        """直接设置模式（模式号需在 config.MODE_NAMES 里）。"""
        if mode in config.MODE_NAMES:
            self._set_mode(mode)

    def cycle_mode(self):
        """循环切换模式（长按 BOOT 手动切换用）：0->1->2->3->0。"""
        self._set_mode((self.mode + 1) % len(config.MODE_NAMES))

    def color_keys(self):
        """当前模式要识别的颜色 key 列表。"""
        return list(config.MODE_COLORS.get(self.mode, []))

    def candidate(self, detections, person):
        """根据当前模式和各检测结果，返回本帧候选 (marker, blob)。

        detections: {颜色 key: blob}（只含当前模式扫到的颜色）
        person    : ai_detector.detect_person 的结果（Detection 或 None）
        """
        if self.mode == config.MODE_TASK:
            # 优先级：橙(救人) > 紫(送物资)。两种颜色圆一般不共存，只取一个。
            blob = detections.get(config.KEY_ORANGE)
            if blob is not None:
                return config.MARKER_TASK_RESCUE, blob
            blob = detections.get(config.KEY_PURPLE)
            if blob is not None:
                return config.MARKER_TASK_DELIVER, blob
            return config.MARKER_NONE, None

        if self.mode == config.MODE_RESCUE:
            # 优先级：危险 > 预警 > 复位牌 > 人（救援目标）
            # 【复位牌必须排在「人」前面】
            # candidate() 每帧只返回优先级最高的一个标记，而 pick() 要求连续
            # CONFIRM_FRAMES 帧同一个标记才确认。救援演示时「人」通常一直在
            # 画面里，复位牌若排在最后就一帧候选都拿不到，于是「识别到蓝色、
            # 屏上显示 RESET，但永远不切回 TASK」。
            blob = detections.get(config.KEY_DANGER)
            if blob is not None:
                return config.MARKER_DANGER, blob
            blob = detections.get(config.KEY_WARNING)
            if blob is not None:
                return config.MARKER_WARNING, blob
            blob = detections.get(config.KEY_RESET)
            if blob is not None:
                return config.MARKER_RESET, blob
            if person is not None:
                return config.MARKER_PERSON, person
            return config.MARKER_NONE, None

        if self.mode == config.MODE_DELIVER:
            # 优先级：危险 > 预警 > 复位牌 > 物资点（同 RESCUE：复位牌不能垫底）
            blob = detections.get(config.KEY_DANGER)
            if blob is not None:
                return config.MARKER_DANGER, blob
            blob = detections.get(config.KEY_WARNING)
            if blob is not None:
                return config.MARKER_WARNING, blob
            blob = detections.get(config.KEY_RESET)
            if blob is not None:
                return config.MARKER_RESET, blob
            blob = detections.get(config.KEY_SUPPLY)
            if blob is not None:
                return config.MARKER_SUPPLY, blob
            return config.MARKER_NONE, None

        return config.MARKER_NONE, None

    def confirm_frames(self):
        """当前模式判定「确认」需要的连续帧数。"""
        return config.CONFIRM_FRAMES

    def pick(self, marker, blob, need=None):
        """去抖：连续 need 帧同一标记才确认，返回 (marker, blob)。

        need 不传时按当前模式取（见 confirm_frames）。
        """
        frames = self.confirm_frames() if need is None else need

        if marker == self._last_marker:
            self._hold += 1
        else:
            self._last_marker = marker
            self._hold = 1

        if self._hold >= frames:
            return marker, blob
        return config.MARKER_NONE, None

    def auto_transition(self, marker):
        """根据确认的标记自动切换模式，返回是否发生切换。

        task + 橙 -> rescue；task + 紫 -> deliver；
        rescue/deliver + 复位牌 -> task。

        任务派发（橙/紫）也要连续识别 TASK_TRANSITION_MS 再切：期间持续上报
        marker 1/2，让 STM32 端能凑够确认帧（和复位牌同理，避免单帧误派发）。
        复位牌（蓝）要连续识别 RESET_TRANSITION_MS 再切：期间 K210 保持在
        当前模式继续上报 marker 7，让 STM32 能凑够确认帧，否则第一帧就切走
        会导致 marker 7 只发 1 帧、STM32 永远确认不了复位。

        冷却期只拦「送物资派发」：切回 TASK 后复位牌(蓝)的阴影/边缘可能被误判成
        紫/黑（送物资）而立刻又切走，所以 TASK -> deliver 要冷却；
        TASK -> rescue（橙）和 复位牌（RESCUE/DELIVER -> TASK）都不受冷却限制。
        """
        now = time.ticks_ms()

        changed = False
        if self.mode == config.MODE_TASK:
            self._reset_since = -1
            if marker in (config.MARKER_TASK_RESCUE, config.MARKER_TASK_DELIVER):
                # 紫/黑 -> 送物资：冷却期拦一下（蓝牌阴影可能被误判成黑）；
                # 橙 -> 救人：不受冷却限制。
                if marker == config.MARKER_TASK_DELIVER and \
                        time.ticks_diff(self._cooldown_until, now) > 0:
                    self._task_marker = config.MARKER_NONE
                    self._task_since = -1
                    return False
                # 任务牌也像复位牌一样：连续识别 TASK_TRANSITION_MS 才切模式，
                # 期间持续上报 marker 1/2，供 STM32 端多帧确认。
                if marker != self._task_marker:
                    self._task_marker = marker
                    self._task_since = now
                if time.ticks_diff(now, self._task_since) >= config.TASK_TRANSITION_MS:
                    if marker == config.MARKER_TASK_RESCUE:
                        self._set_mode(config.MODE_RESCUE)
                    else:
                        self._set_mode(config.MODE_DELIVER)
                    changed = True
            else:
                self._task_marker = config.MARKER_NONE
                self._task_since = -1
        elif self.mode in (config.MODE_RESCUE, config.MODE_DELIVER):
            if marker == config.MARKER_RESET:
                if self._reset_since < 0:
                    self._reset_since = now
                if time.ticks_diff(now, self._reset_since) >= config.RESET_TRANSITION_MS:
                    self._set_mode(config.MODE_TASK)
                    changed = True
            else:
                self._reset_since = -1

        if changed:
            self._reset_since = -1
            self._task_marker = config.MARKER_NONE
            self._task_since = -1
            self._cooldown_until = time.ticks_add(now,
                                                  config.TRANSITION_COOLDOWN_MS)
        return changed

    def _set_mode(self, mode):
        """切换模式并清空去抖状态，让新模式从干净的帧开始。"""
        self.mode = mode
        self._last_marker = config.MARKER_NONE
        self._hold = 0
        self._task_marker = config.MARKER_NONE
        self._task_since = -1
