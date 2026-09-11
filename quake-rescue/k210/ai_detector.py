# -*- coding: utf-8 -*-
"""AI 检测：人检测（VOC20）。

任务派发（救人/送物资）已从数字识别改为颜色圆检测（橙=救人/紫=送物资），
颜色检测见 detector.py，不再需要数字 KPU 模型。
其余标记（物资点/危险/预警/复位）也用颜色，见 detector.py。
"""

import gc

import config
from maix import KPU
from image import Image


class Detection:
    """轻量检测框，接口对齐颜色 blob（cx/cy/rect/pixels），方便复用显示和上报。"""

    def __init__(self, x, y, w, h, score):
        self._x = int(x)
        self._y = int(y)
        self._w = int(w)
        self._h = int(h)
        self.score = score

    def cx(self):
        return self._x + self._w // 2

    def cy(self):
        return self._y + self._h // 2

    def x(self):
        return self._x

    def y(self):
        return self._y

    def w(self):
        return self._w

    def h(self):
        return self._h

    def rect(self):
        return (self._x, self._y, self._w, self._h)

    def pixels(self):
        return self._w * self._h


_kpu_person = None
_person_img = None


def _load_person():
    global _kpu_person, _person_img
    if _kpu_person is not None:
        return
    _kpu_person = KPU()
    _kpu_person.load_kmodel(config.VOC20_PATH)
    _kpu_person.init_yolo2(config.VOC20_ANCHOR, anchor_num=5,
                           img_w=320, img_h=240,
                           net_w=320, net_h=256,
                           layer_w=10, layer_h=8,
                           threshold=config.PERSON_THRESHOLD,
                           nms_value=config.VOC20_NMS, classes=20)
    # 320x256 的 AI 图（网络尺寸），放在 Python 堆上不占图像缓冲池。
    # 【必须与参考代码一致】见「实验六_自主学习分类/voc20_object_detector.py」：
    #   od_img = Image(size=(320, 256), copy_to_fb=False)
    #   od_img.draw_image(img, 0, 0)   # img 是 320x240 的摄像头画面
    #   od_img.pix_to_ai()
    #   kpu.run_with_output(od_img)
    # img_w/img_h 是「摄像头画面尺寸」(320x240)，net_w/net_h 才是网络尺寸(320x256)，
    # 两者本来就不相等；把 AI 图改成 224x224 会导致行宽对不上、检不到人。
    _person_img = Image(size=(320, 256), copy_to_fb=False)
    print("voc20 model loaded")


def _unload_person():
    global _kpu_person, _person_img
    if _kpu_person is None:
        return
    try:
        _kpu_person.deinit()
    except Exception:
        pass
    _kpu_person = None
    _person_img = None
    gc.collect()
    print("voc20 model freed")


def apply_mode(mode):
    """按模式只保留需要的模型（可以每帧调用，状态不对才动作）。

    任务派发已改为颜色检测，不再需要数字 KPU 模型；只有救人模式才加载
    VOC20 人检测模型。VOC20(1.5MB) 占的模型缓冲和 find_blobs 用的是同一块
    SRAM，人模型常驻时颜色检测的临时缓冲会被挤掉（「一进救人模式就
    Out of Memory」的根因），所以只在救人模式按需加载。
    """
    if mode == config.MODE_RESCUE:
        _load_person()
    else:
        _unload_person()


def init():
    """上电初始化：只加载当前模式真正需要的模型。"""
    apply_mode(config.INITIAL_MODE)


def deinit():
    """释放全部模型（脚本结束/异常退出时调用）。

    K210 上 KPU 模型占的内存不会因为脚本崩了自动归还，于是「崩溃后直接
    再运行」常常在 load_kmodel 处报 "model buffer memory allocation failed"。
    在 main 的 finally 里调用本函数，可以避免这种脏状态。
    """
    global _person_img
    _unload_person()
    _person_img = None


def detect_person(img):
    """检测画面里的人（VOC20 person 类），返回最高分的 Detection，或 None。"""
    if _kpu_person is None:                  # 当前模式没加载人检测模型
        return None
    # 先把摄像头画面整幅拷进 320x256 的 AI 图，再转 AI 格式：
    #   - 与参考代码 voc20_object_detector.py 的写法一致（320x240 放左上角，下面 16 行是留白）；
    #   - pix_to_ai() 是原地转换，直接对摄像头画面调用会把 LCD 要显示的那张图弄花。
    _person_img.draw_image(img, 0, 0)
    _person_img.pix_to_ai()
    _kpu_person.run_with_output(_person_img)
    dect = _kpu_person.regionlayer_yolo2() or ()   # [(x,y,w,h,class,score), ...]

    best = None
    for item in dect:
        if len(item) < 6:
            continue
        cls = int(item[4])
        score = item[5]
        if cls == config.PERSON_CLASS and score >= config.PERSON_THRESHOLD:
            if best is None or score > best[5]:
                best = item

    if best is None:
        return None
    det = Detection(best[0], best[1], best[2], best[3], best[5])
    # 尺寸过滤：人像太小（远处/误检）不算，要求与颜色圆尺寸相当。
    if det.pixels() < config.PERSON_MIN_AREA:
        return None
    return det
