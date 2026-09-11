# -*- coding: utf-8 -*-
"""摄像头初始化与取帧。"""

import sensor
import time
import config


def init():
    """初始化摄像头，返回用于 FPS 统计的 clock。"""
    sensor.reset()
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.QVGA)          # 320 x 240 全画幅
    # 【注意】这里不要加 set_windowing((224,224))：
    #   1) VOC20 人检测要求输入是 320x240（见 ai_detector.init_yolo2 的 img_w/img_h，
    #      与「实验六_自主学习分类/voc20_object_detector.py」完全一致），AI 图按
    #      320 宽排布，行宽对不上会直接检不到人；
    #   2) windowing 只取传感器中心区域，横向视野少 30%，对巡线找人不利。
    # 颜色检测的内存问题改用 config.ROI 的子区域解决（find_blobs 的临时缓冲按 ROI 面积走）。
    # 只用一块 framebuffer：省下一整块 320x240x2 ≈ 150KB。
    # 若启动时报 "model buffer memory allocation failed"，把下面两行注释掉即可。
    try:
        sensor.set_framebuffers(1)
    except Exception:
        pass
    sensor.set_vflip(config.SENSOR_VFLIP)
    sensor.set_hmirror(config.SENSOR_HMIRROR)
    sensor.skip_frames(time=500)
    # 关闭自动白平衡/增益，让颜色阈值更稳定（如环境光不稳可改回 True）
    sensor.set_auto_gain(False)
    sensor.set_auto_whitebal(False)
    return time.clock()


def snapshot():
    """取一帧图像。"""
    return sensor.snapshot()
