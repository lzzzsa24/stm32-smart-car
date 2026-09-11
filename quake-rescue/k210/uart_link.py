# -*- coding: utf-8 -*-
"""与 STM32 的串口通信（K210 8脚=TX、6脚=RX，115200 8-N-1）。

注意：STM32 端对 cx/cy/area 做的是「范围校验」——只要越界（cx>319 或
cy>239 等），整帧会被丢弃。而 STM32 实际只按 marker 做决策，坐标只看不判。
所以这里一律把坐标钳到合法范围，宁可坐标不精确，也不能因越界丢帧。
"""

from machine import UART
from fpioa_manager import fm
import config

_IMG_W = 319        # STM32 允许的 cx 上限
_IMG_H = 239        # STM32 允许的 cy 上限
_AREA_MAX = 100000  # STM32 允许的 area 上限


def _clamp(value, low, high):
    if value < low:
        return low
    if value > high:
        return high
    return value


def init():
    """初始化 UART1，返回 uart 对象。"""
    fm.register(config.UART_TX_PIN, fm.fpioa.UART1_TX, force=True)
    fm.register(config.UART_RX_PIN, fm.fpioa.UART1_RX, force=True)
    return UART(UART.UART1, config.UART_BAUD, 8, 0, 1,
                timeout=1000, read_buf_len=4096)


def send_frame(uart, mode, marker, blob):
    """发送一帧识别结果给 STM32。

    协议：$T,<mode>,<marker>,<cx>,<cy>,<area>#
      mode   : 当前模式 0 无 / 1 派发 / 2 救人 / 3 送物资
      marker : 标记 -1 无 / 1 救人(橙色圆) / 2 送物资(紫色圆) / 3 人(VOC20) /
               4 物资点(绿) / 5 危险区(红) / 6 余震预警(黄) / 7 复位牌(蓝)
      cx,cy  : 标记中心坐标（无标记时为 -1）
      area   : 标记像素面积（无标记时为 -1）
    """
    if blob is None:
        cx = cy = area = -1
    else:
        # 兜底钳位：保证帧一定落在 STM32 的合法区间内（越界会被整帧丢弃）。
        # 画面是 320x240，正常不会越界；STM32 只看 marker，坐标精度无影响。
        cx = _clamp(blob.cx(), 0, _IMG_W)
        cy = _clamp(blob.cy(), 0, _IMG_H)
        area = _clamp(blob.pixels(), 0, _AREA_MAX)

    frame = "$T,%d,%d,%d,%d,%d#\n" % (mode, marker, cx, cy, area)
    uart.write(frame)
