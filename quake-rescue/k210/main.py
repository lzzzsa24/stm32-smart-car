# -*- coding: utf-8 -*-
"""
K210 救援任务识别（多文件结构）—— 入口
========================================
任务派发用颜色圆（橙=救人 紫=送物资），救人用 VOC20 人检测，其余用颜色。

模式（长按 BOOT 手动循环）：
  0 无模式  ：不识别任何东西
  1 任务派发：橙(救人) / 紫(送物资)
  2 救人    ：VOC20 识别人 + 红(危险) + 黄(预警) + 蓝(复位)
  3 送物资  ：绿(物资点) + 红(危险) + 黄(预警) + 蓝(复位)

自动切换：
  task 模式 + 橙色圆          -> 自动进 rescue
  task 模式 + 紫色圆          -> 自动进 deliver
  rescue/deliver + 蓝(复位牌)  -> 自动回 task

短按 BOOT：切叠加层显示/隐藏。
串口协议：$T,<mode>,<marker>,<cx>,<cy>,<area>#
接线：K210 8脚(TX)->STM32 PD6(RX)，6脚(RX)->STM32 PD5(TX)，共地，115200 8-N-1。
"""

import sys
import gc
import time


def _setup_path():
    """把本文件所在目录及常见位置加入 sys.path，避免 "no module named ..."。

    在 CanMV IDE 里直接运行本脚本时，sys.path 未必包含本文件所在目录，
    于是 `import config / ai_detector` 会报错。这里做兜底：

      * 本脚本所在目录、以及 /sd/k210 放到搜索路径**最前面**
        （防止 SD 根目录里旧版本的 config.py 之类抢先被加载）；
      * /sd、/flash 等放在最后仅作兜底。

    总之：9 个 .py 放同一个文件夹即可正常导入。
    """
    front = []
    try:
        here = __file__
    except NameError:
        here = None
    if here:
        d = str(here).replace("\\", "/").rsplit("/", 1)[0]
        if d:
            front.append(d)
    front.append("/sd/k210")
    for d in reversed(front):
        if d not in sys.path:
            sys.path.insert(0, d)
    for d in ("/sd", "/flash/k210", "/flash", ""):
        if d not in sys.path:
            sys.path.append(d)


_setup_path()

try:
    import lcd
    import config
    import camera
    import button
    import detector
    import ai_detector
    import task_engine
    import uart_link
    import display
except ImportError as err:
    print("IMPORT FAIL:", err)
    print("sys.path =", sys.path)
    print("请把 k210/ 里的 9 个 .py 放到板子同一个文件夹，再从该文件夹运行 main.py")
    raise


def main():
    lcd.init(freq=15000000)
    lcd.clear(lcd.BLACK)
    clock = camera.init()
    uart = uart_link.init()
    ai_detector.init()
    engine = task_engine.TaskEngine()
    boot = button.BootButton()
    show_overlay = config.SHOW_OVERLAY_DEFAULT

    banner = ""
    banner_until = 0

    now0 = time.ticks_ms()
    last_send = now0
    last_display = now0 - config.DISPLAY_INTERVAL_MS
    last_debug = now0 - config.DEBUG_INTERVAL_MS
    last_gc = now0

    print("READY; color+VOC20; short=overlay, long=mode")
    print("free mem after init: %d bytes" % gc.mem_free())

    while True:
        now = time.ticks_ms()
        # 每帧都回收一次：人检测/颜色检测每帧都会分配图像缓冲，
        # 这些缓冲靠 GC 释放；累积不回收会耗尽缓冲池并抛
        # "MemoryError: Out of Memory! ... reduce the resolution"。
        gc.collect()
        last_gc = now

        clock.tick()
        img = camera.snapshot()

        # 1. 按键：短按切叠加层，长按手动切模式
        ev = boot.event()
        if ev == 'short':
            show_overlay = not show_overlay
            print("show_overlay =", show_overlay)
        elif ev == 'long':
            engine.cycle_mode()
            banner = "-> %s" % config.MODE_NAMES.get(engine.mode, "?")
            banner_until = now + config.BANNER_MS
            print("manual -> mode %d (%s)" %
                  (engine.mode, config.MODE_NAMES.get(engine.mode, "?")))

        # 模型按需加载：VOC20 1.5MB 常驻会挤占颜色检测 find_blobs 需要的
        # 临时缓冲（「进救人模式就 Out of Memory」的根因）。
        # apply_mode 每帧调用是安全的：状态已经对了就什么都不做。
        ai_detector.apply_mode(engine.mode)

        # 2. 按模式做检测（无模式时什么都不识别）
        detections = {}
        person = None

        if engine.mode in (config.MODE_TASK, config.MODE_RESCUE, config.MODE_DELIVER):
            detections = detector.scan(img, engine.color_keys())
        if engine.mode == config.MODE_RESCUE:
            person = ai_detector.detect_person(img)
        # MODE_NONE：跳过所有检测

        # 3. 合成候选标记 + 去抖
        raw_marker, raw_blob = engine.candidate(detections, person)
        marker, blob = engine.pick(raw_marker, raw_blob)

        # 4. 标记驱动的自动切换
        if engine.auto_transition(marker):
            banner = "-> %s" % config.MODE_NAMES.get(engine.mode, "?")
            banner_until = now + config.BANNER_MS
            print("auto -> mode %d (%s)" %
                  (engine.mode, config.MODE_NAMES.get(engine.mode, "?")))

        # 5. 串口上报（按间隔）
        if time.ticks_diff(now, last_send) >= config.SEND_INTERVAL_MS:
            uart_link.send_frame(uart, engine.mode, marker, blob)
            last_send = now
            if time.ticks_diff(now, last_debug) >= config.DEBUG_INTERVAL_MS:
                # 带上剩余内存：崩溃前那几行的数值能直接看出内存是不是在被吃光
                print("mode=%d marker=%d mem=%d" %
                      (engine.mode, marker, gc.mem_free()))
                last_debug = now

        # 6. 显示（按间隔）
        if time.ticks_diff(now, last_display) >= config.DISPLAY_INTERVAL_MS:
            if banner and time.ticks_diff(now, banner_until) > 0:
                banner = ""
            if show_overlay:
                display.draw(img, detections, person,
                             engine.mode, marker, int(clock.fps()), banner)
            lcd.display(img)
            last_display = now

        # 及时释放，避免内存累积
        del img


try:
    main()
except Exception as err:
    # 把完整 traceback 打到串口终端（含出错的文件名和行号），方便定位
    print("========== ERROR ==========")
    sys.print_exception(err)
    print("===========================")
finally:
    # 正常结束或崩溃退出都释放 KPU 模型：否则下次运行会在加载模型时报
    # "model buffer memory allocation failed"（参考实验六的 finally: kpu.deinit()）
    ai_detector.deinit()
    gc.collect()
