# -*- coding: utf-8 -*-
"""颜色标记检测：按模式只检测指定颜色。"""

import config


def scan(img, keys):
    """只对 keys 里的颜色做 find_blobs。

    参数 keys: 本模式需要识别的颜色 key 列表（config.THRESHOLDS 的键名）。
    返回 dict：{颜色 key: 该颜色最大的 blob（或 None）}。
    """
    result = {}
    for key in keys:
        threshold = config.THRESHOLDS[key]
        blobs = img.find_blobs([threshold],
                               roi=config.ROI,
                               pixels_threshold=config.PIXELS_THRESHOLD,
                               area_threshold=config.AREA_THRESHOLD,
                               merge=config.MERGE,
                               margin=config.MARGIN)
        result[key] = max(blobs, key=lambda b: b.pixels()) if blobs else None
    return result
