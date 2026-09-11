# -*- coding: utf-8 -*-
"""颜色标定：按固定顺序「红→橙→黄→绿→蓝→紫」采样，算出 LAB 阈值，
带颜色名和序号写入 SD 卡文本文件 /sd/color_calib.txt。

【用法】
  1. CanMV IDE 新建文件，粘入全部内容，点「运行」。
  2. 按 LCD 提示依次出示色卡：红 → 橙 → 黄 → 绿 → 蓝 → 紫。
     每张卡摆满屏幕中间的黄色采样框，摆稳后短按 BOOT 键采样。
  3. 六张采完自动把结果写入 /sd/color_calib.txt，格式：
        1  RED     (34, 54, 50, 78, 22, 50)   # 红·危险区 L=44 A=65 B=36
        2  ORANGE  (31, 54, 22, 60, 24, 51)   # 橙·救人任务 L=43 A=42 B=38
        ...
     括号里就是可直接填进 config.py 的 (L_min,L_max,A_min,A_max,B_min,B_max)。

【阈值怎么算】
  每张卡采 SAMPLES 帧，取均值 μ 和标准差 σ，阈值 = μ ± 2σ，再往外放宽
  PAD_*（L 放宽 6、A/B 各放宽 8），并夹在合法范围内。

【注意】
  - 用演示时同样的距离和灯光；脚本已关自动增益/白平衡（和项目一致）。
  - 色卡要摆满采样框，否则背景混进来数值会偏。
"""

import time
import lcd
import sensor
from board import board_info
from fpioa_manager import fm
from maix import GPIO

# ---------------- 可调参数 ----------------
SAMPLE_W = 160      # 采样框边长（居中）。色卡要摆满这个框
SAMPLES  = 15       # 每张卡采样的帧数
PAD_L    = 6        # 阈值在「均值 ± 2σ」之外再放宽的量：越大越宽松（越易误检）
PAD_A    = 8
PAD_B    = 8
FILE     = "/sd/color_calib.txt"

# (英文名, 中文含义) —— 顺序就是标定顺序，也对应 txt 里的序号 1~6
CARDS = (
    ("RED",    "红·危险区"),
    ("ORANGE", "橙·救人任务"),
    ("YELLOW", "黄·余震预警"),
    ("GREEN",  "绿·物资点"),
    ("BLUE",   "蓝·复位牌"),
    ("PURPLE", "紫·送物资任务"),
)

ROI = ((320 - SAMPLE_W) // 2, (240 - SAMPLE_W) // 2, SAMPLE_W, SAMPLE_W)
# ------------------------------------------

lcd.init()
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)      # 与项目一致：320x240
sensor.skip_frames(time=1000)
sensor.set_auto_gain(False)            # 必须关，否则标定值无效
sensor.set_auto_whitebal(False)

fm.register(board_info.BOOT_KEY, fm.fpioa.GPIOHS0)
boot = GPIO(GPIO.GPIOHS0, GPIO.IN)


def clamp(v, lo, hi):
    return lo if v < lo else (hi if v > hi else v)


def make_thr(ml, ma, mb, sl, sa, sb):
    """由均值 μ 和标准差 σ 算 (L_min,L_max,A_min,A_max,B_min,B_max)。"""
    return (int(clamp(ml - 2 * sl - PAD_L, 0, 100)),
            int(clamp(ml + 2 * sl + PAD_L, 0, 100)),
            int(clamp(ma - 2 * sa - PAD_A, -128, 127)),
            int(clamp(ma + 2 * sa + PAD_A, -128, 127)),
            int(clamp(mb - 2 * sb - PAD_B, -128, 127)),
            int(clamp(mb + 2 * sb + PAD_B, -128, 127)))


def wait_press(idx, name):
    """预览并等待用户短按 BOOT。"""
    while boot.value() == 0:                 # 确保按键已松开
        time.sleep_ms(10)
    while boot.value() == 1:                 # 等按下
        img = sensor.snapshot()
        img.draw_rectangle(ROI, color=(255, 255, 0), thickness=2)
        img.draw_string(4, 4, "[%d/%d] %s" % (idx, len(CARDS), name),
                        color=(255, 255, 0), scale=2)
        img.draw_string(4, 34, "fill box, press BOOT",
                        color=(0, 255, 255), scale=1)
        lcd.display(img)
    while boot.value() == 0:                 # 等松开
        time.sleep_ms(10)


def sample_color(idx, name):
    """采样一张卡，返回 (ml, ma, mb, sd_l, sd_a, sd_b)。"""
    sl = sa = sb = 0.0
    dl = da = db = 0.0
    for i in range(SAMPLES):
        img = sensor.snapshot()
        st = img.get_statistics(roi=ROI)
        sl += st.l_mean();  sa += st.a_mean();  sb += st.b_mean()
        dl += st.l_stdev(); da += st.a_stdev(); db += st.b_stdev()
        img.draw_rectangle(ROI, color=(0, 255, 0), thickness=2)
        img.draw_string(4, 4, "%s sampling %d/%d" % (name, i + 1, SAMPLES),
                        color=(0, 255, 0), scale=2)
        lcd.display(img)
        time.sleep_ms(30)

    n = float(SAMPLES)
    return (sl / n, sa / n, sb / n, dl / n, da / n, db / n)


def main():
    records = []      # (idx, name, meaning, thr, ml, ma, mb)
    print("颜色标定开始：红→橙→黄→绿→蓝→紫")

    for i, (name, meaning) in enumerate(CARDS):
        idx = i + 1
        wait_press(idx, name)
        ml, ma, mb, sd_l, sd_a, sd_b = sample_color(idx, name)
        thr = make_thr(ml, ma, mb, sd_l, sd_a, sd_b)
        records.append((idx, name, meaning, thr, ml, ma, mb))

        print("#%d %-7s %-28s # %s L=%.1f A=%.1f B=%.1f  σ=(%.1f,%.1f,%.1f)"
              % (idx, name, str(thr), meaning, ml, ma, mb, sd_l, sd_a, sd_b))
        if max(sd_l, sd_a, sd_b) > 25:
            print("   !! 波动偏大(σ>25)，卡可能没摆满框或手在动，建议重采这张")

    # ---- 蓝/紫提示：两者都是蓝系（B 都偏负），只能靠 L（亮度）分开 ----
    blue = purple = None
    for r in records:
        if r[1] == "BLUE":
            blue = r
        elif r[1] == "PURPLE":
            purple = r
    if blue is not None and purple is not None:
        bl, bh = blue[3][0], blue[3][1]     # 蓝 L 区间
        pl, ph = purple[3][0], purple[3][1] # 紫 L 区间
        if bl <= ph and pl <= bh:           # 两个 L 区间有交集 -> 会串
            print("!! 蓝 L(%d~%d) 与紫 L(%d~%d) 重叠：这俩会互相串。"
                  "请按实测亮度把亮的那个 L 区间整体上移、暗的往下移，"
                  "中间留 4 格空隙（比如 蓝 L 31~36、紫 L 40~47）。"
                  % (bl, bh, pl, ph))

    # ---- 写入 txt（覆盖写：每次运行得到干净的 1~6）----
    try:
        f = open(FILE, "w")
        for idx, name, meaning, thr, ml, ma, mb in records:
            f.write("%d  %-7s %-28s # %s L=%.0f A=%.0f B=%.0f\n"
                    % (idx, name, str(thr), meaning, ml, ma, mb))
        f.close()
        print("")
        print("已写入 %s（序号 1~6）" % FILE)
    except Exception as e:
        print("")
        print("!! 写文件失败：%s" % e)

    img = sensor.snapshot()
    img.draw_string(60, 100, "DONE", color=(0, 255, 0), scale=3)
    lcd.display(img)
    print("标定完成。")


main()
