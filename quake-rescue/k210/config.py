# -*- coding: utf-8 -*-
"""
全局配置：颜色阈值、AI 模型路径、模式门控、检测参数、时序、串口。

只需改这个文件就能适配你的实物标记和环境光线。
"""

# ---------- 摄像头 ----------
SENSOR_VFLIP   = 0      # 画面垂直翻转（按摄像头安装方向调整）
SENSOR_HMIRROR = 0      # 画面水平镜像

# 屏幕叠加层默认开关；运行中短按 BOOT 键可切换显示/隐藏
SHOW_OVERLAY_DEFAULT = True

# ---------- 颜色阈值 (L_min, L_max, A_min, A_max, B_min, B_max) ----------
# LAB 三通道含义（CanMV 范围）：
#   L：亮度   0(黑) ~ 100(白)
#   A：红绿   -128(绿) ~ +127(红/品红)
#   B：蓝黄   -128(蓝) ~ +127(黄)
#
# 下面 6 组来自 color_calib.txt 的最新实测标定（行尾注释是实测均值）。
# 通道分工：A/B 决定「是什么颜色」，L 只管亮度是否合理。
COLOR_RED    = (33, 53, 54, 84, 26, 54)        # 红：危险区   实测 L=44 A=69 B=41
COLOR_YELLOW = (52, 70, -16, 28, 38, 62)       # 黄：余震预警 实测 L=57 A=6 B=50（已调亮）
COLOR_GREEN  = (34, 62, -51, -11, 16, 44)      # 绿：物资点   实测 L=49 A=-32 B=31
COLOR_BLUE   = (10, 30, 15, 46, -50, -25)      # 蓝：复位牌   实测 L=21 A=31 B=-38
COLOR_ORANGE = (30, 49, 16, 55, 23, 47)        # 橙：救人任务 实测 L=40 A=36 B=35
COLOR_PURPLE = (0, 15, -12, 12, -12, 12)       # 黑：送物资（原紫，很黑/调窄）

# ---------- 颜色标记语义 ----------
# 六种颜色圆对应的语义标记（颜色圆，原数字识别已移除）：
KEY_SUPPLY  = "supply"    # 绿 -> 物资点（送物资目标）
KEY_DANGER  = "danger"    # 红 -> 危险区
KEY_WARNING = "warning"   # 黄 -> 余震预警
KEY_RESET   = "reset"     # 蓝 -> 复位牌（回 task 模式）
KEY_ORANGE  = "orange"    # 橙 -> 救人任务（原数字 1）
KEY_PURPLE  = "purple"    # 紫 -> 送物资任务（原数字 2）

THRESHOLDS = {
    KEY_SUPPLY:  COLOR_GREEN,
    KEY_DANGER:  COLOR_RED,
    KEY_WARNING: COLOR_YELLOW,
    KEY_RESET:   COLOR_BLUE,
    KEY_ORANGE:  COLOR_ORANGE,
    KEY_PURPLE:  COLOR_PURPLE,
}

# 屏幕上显示的标签（CanMV 默认字库不支持中文，用英文最稳）
LABELS = {
    KEY_SUPPLY:  "SUPPLY",
    KEY_DANGER:  "DANGER",
    KEY_WARNING: "WARNING",
    KEY_RESET:   "RESET",
    KEY_ORANGE:  "RESCUE",
    KEY_PURPLE:  "DELIVER",
}

# 画框颜色 (RGB565)，与实际卡颜色对应，方便视频里区分
DRAW_COLOR = {
    KEY_SUPPLY:  (0, 255, 0),
    KEY_DANGER:  (255, 0, 0),
    KEY_WARNING: (255, 255, 0),
    KEY_RESET:   (0, 0, 255),
    KEY_ORANGE:  (255, 128, 0),
    KEY_PURPLE:  (200, 0, 255),
}

# ---------- 任务派发：颜色圆（原数字识别已移除） ----------
# 数字识别模型 cls.kmodel 连续训练几版都无法稳定区分 1/2（模型本身偏置/退化），
# 现改为颜色圆：橙色=救人（原数字1）、紫色=送物资（原数字2），用颜色阈值识别，
# 不再加载任何 KPU 数字模型。阈值见上面 COLOR_ORANGE / COLOR_PURPLE。

# ---------- AI 模型：人检测（VOC20） ----------
VOC20_PATH       = "/sd/KPU/voc20_object_detect/voc20_detect.kmodel"
VOC20_ANCHOR     = (1.3221, 1.73145, 3.19275, 4.00944, 5.05587,
                    8.09892, 9.47112, 4.84053, 11.2364, 10.0071)  # 官方原值勿改
PERSON_CLASS     = 14     # VOC20 里 "person"（人）的类别号
PERSON_THRESHOLD = 0.5    # 人检测置信度阈值
PERSON_MIN_AREA  = 4000   # 人像最小外接框面积(w*h)：太小(远处/误检)忽略，与颜色圆尺寸相当
VOC20_NMS        = 0.2    # 非极大值抑制

# ---------- 模式门控（无模式时什么都不识别） ----------
MODE_NONE    = 0   # 无模式 / 停车：不识别任何东西
MODE_TASK    = 1   # 任务派发：橙(救人) / 紫(送物资)
MODE_RESCUE  = 2   # 救人：VOC20 识别人 + 红(危险) + 黄(预警) + 蓝(复位)
MODE_DELIVER = 3   # 送物资：绿(物资点) + 红(危险) + 黄(预警) + 蓝(复位)

# 上报给 STM32 的标记 ID
MARKER_NONE          = -1
MARKER_TASK_RESCUE   = 1    # 橙色圆 -> 救人（原数字「1」）
MARKER_TASK_DELIVER  = 2    # 紫色圆 -> 送物资（原数字「2」）
MARKER_PERSON        = 3    # 人（VOC20）
MARKER_SUPPLY        = 4    # 物资点（绿）
MARKER_DANGER        = 5    # 危险区（红）
MARKER_WARNING       = 6    # 余震预警（黄）
MARKER_RESET         = 7    # 复位牌（蓝）

# 每个模式要识别的颜色 key（供 detector.scan 用；识别优先级见 task_engine.py）
MODE_COLORS = {
    MODE_NONE:    [],
    MODE_TASK:    [KEY_ORANGE, KEY_PURPLE],
    MODE_RESCUE:  [KEY_DANGER, KEY_WARNING, KEY_RESET],
    MODE_DELIVER: [KEY_SUPPLY, KEY_DANGER, KEY_WARNING, KEY_RESET],
}

# 模式名（显示/调试用）
MODE_NAMES = {
    MODE_NONE:    "NONE",
    MODE_TASK:    "TASK",
    MODE_RESCUE:  "RESCUE",
    MODE_DELIVER: "DELIVER",
}

# 上电初始模式：想上电就进入任务派发用 MODE_TASK；想上电什么都不识别用 MODE_NONE
INITIAL_MODE = MODE_TASK

# ---------- 检测参数 ----------
# 颜色检测区域。【面积决定内存】：find_blobs 的临时缓冲按 ROI 面积走，
# 整帧 320x240(=76800) 曾把图像缓冲池撑爆（MemoryError: Out of Memory）。
# 这里用「全宽 + 中间 160 行」= 51200，与实测跑通的 224x224(=50176) 同量级，
# 但保留完整横向视野。参考代码（test-exp7）用的也是这种全宽子区域 (0,40,320,200)。
ROI               = (0, 40, 320, 160)
# 色块最小像素/外接面积（尺寸阈值）。颜色圆直径约 = 屏高 240 的 1/3 ≈ 80px，
# 整圆像素 ≈ π*40² ≈ 5000、外接框 ≈ 80*80 = 6400。
# 阈值已调低到 2000：能认出更远/更小的色块（直径约 50px 起），
# 同时仍过滤背景噪点/反光。若误检变多可调回 3000。
PIXELS_THRESHOLD  = 2000
AREA_THRESHOLD    = 2000
MERGE             = True
MARGIN            = 10

# 各类标记的确认帧数。已改为 1：K210 端不延迟，识别到第一帧就上报，
# 由 STM32 端做「停车后若干帧确认」（见 quake_config.h 的 QUAKE_MARKER_CONFIRM_FRAMES）。
CONFIRM_FRAMES = 1

# 自动切换模式后的冷却时间（只拦「任务派发」）：切回 TASK 时蓝色复位牌还在画面里，
# 可能被颜色阈值误判成紫（送物资）而立刻又切走（表现为「复位没生效」）。
# 复位牌（rescue/deliver -> task）不受冷却限制，随时能切回。设 0 表示不冷却。
TRANSITION_COOLDOWN_MS = 2500

# 复位牌（蓝）触发「切回 TASK」前，需要连续识别并上报这么久（毫秒）。
# 期间 K210 保持当前模式、持续上报 marker 7，让 STM32 端能凑够
# QUAKE_MARKER_CONFIRM_FRAMES（4 帧 ≈ 200ms）完成「停车后确认」。
# 若设成 0，就会第一帧就切走，marker 7 只发 1 帧，STM32 永远确认不了复位。
RESET_TRANSITION_MS = 300

# 任务牌（橙/紫）触发「切到救人/送物资」前，需要连续识别并上报这么久（毫秒）。
# 期间 K210 保持 TASK 模式、持续上报 marker 1/2，让 STM32 端能凑够
# QUAKE_MARKER_CONFIRM_FRAMES（4 帧 ≈ 200ms）完成多帧确认，避免单帧误派发。
TASK_TRANSITION_MS = 300

# ---------- 时序 ----------
SEND_INTERVAL_MS    = 50              # 串口发送间隔
DISPLAY_INTERVAL_MS = 100             # LCD 刷新间隔
GC_INTERVAL_MS      = 2000            # 垃圾回收间隔
DEBUG_INTERVAL_MS   = 500             # 调试打印间隔
BANNER_MS           = 1500            # 模式切换横幅显示时长

# ---------- 串口 ----------
UART_TX_PIN = 8
UART_RX_PIN = 6
UART_BAUD   = 115200
