# -*- coding: utf-8 -*-
"""LCD 可视化：把识别结果画在画面上，方便录视频时展示。"""

import config


def _mode_name(mode):
    return config.MODE_NAMES.get(mode, "?")


def _event_text(mode, marker):
    """根据「模式 + 标记」给出最醒目的事件文案。"""
    if marker == config.MARKER_DANGER:
        return "DANGER!"
    if marker == config.MARKER_WARNING:
        return "WARNING!"
    if marker == config.MARKER_RESET:
        return "BACK TO TASK"
    if mode == config.MODE_TASK and marker == config.MARKER_TASK_RESCUE:
        return "RESCUE!"       # 橙色圆 -> 救人
    if mode == config.MODE_TASK and marker == config.MARKER_TASK_DELIVER:
        return "DELIVER!"      # 紫色圆 -> 送物资
    if mode == config.MODE_RESCUE and marker == config.MARKER_PERSON:
        return "PERSON FOUND!"
    if mode == config.MODE_DELIVER and marker == config.MARKER_SUPPLY:
        return "DELIVER DONE!"
    return ""


def draw(img, detections, person, mode, marker, fps, banner=""):
    """在图像上叠加检测框、标签、状态栏和事件提示。

    detections : {颜色 key: blob}
    person     : ai_detector.Detection 或 None
    banner     : 模式切换提示（非空时优先显示）
    """
    # 1. 颜色块：画框 + 标签
    for key, blob in detections.items():
        if blob is None:
            continue
        color = config.DRAW_COLOR.get(key, (255, 255, 255))
        img.draw_rectangle(blob.rect(), color=color, thickness=2)
        img.draw_string(blob.x(), max(0, blob.y() - 14),
                        config.LABELS.get(key, key),
                        color=color, scale=2)

    # 2. 人：画粗框 + 标签（VOC20）
    if person is not None:
        img.draw_rectangle(person.rect(), color=(0, 255, 0), thickness=3)
        img.draw_string(person.x(), max(0, person.y() - 16),
                        "PERSON %.2f" % person.score,
                        color=(0, 255, 0), scale=2)

    # 3. 顶部状态栏
    img.draw_string(0, 0, "MODE:%s MARK:%d FPS:%d" %
                    (_mode_name(mode), marker, fps),
                    color=(255, 255, 255), scale=1)

    # 4. 模式切换横幅优先；否则事件文案
    if banner:
        img.draw_string(60, 90, banner, color=(0, 255, 255), scale=2)
    else:
        text = _event_text(mode, marker)
        if text:
            img.draw_string(90, 100, text, color=(255, 0, 0), scale=2)
