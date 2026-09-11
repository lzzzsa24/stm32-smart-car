# 地震灾后救援小车（STM32 端）

在**实验七 `test-exp7-unified-motion-v1` 的循迹代码基础上**，新增地震救援任务逻辑：
K210 用颜色圆派发任务（**橙=救人、黑=送物资**）、用 VOC20 识别人，其余用颜色；STM32 收到 K210 的 `$T` 帧后，
按实验七「模式一」**纯四路循迹**（避障/绕障已关闭），
执行「等任务 → 派发 → 巡逻（救人/送物资）→ 避险 → 复位」的救援流程。

**核心原则：`motion/` 里的循迹、避障、驱动代码是从实验七原样拷贝的，没有改动；
地震项目的新代码全部在 `quake/` 里，只通过 `#include` 调用它们。**

## 独立工程 / 编译与烧录

本目录已配成**独立 STM32 工程**，含 `Core/`（平台：main.h、gpio、hal_msp、system、syscalls/sysmem、
中断文件）、`Drivers/`（ST F1 HAL + CMSIS）、启动文件与链接脚本，入口是 `quake/quake_main.c`（其内是 `main()`）。

**编译（需本机 STM32CubeIDE 1.16.0，脚本用其自带 GNU ARM 工具链）：**

```powershell
pwsh -File build_quake.ps1
# 或： 在 PowerShell 里  Set-ExecutionPolicy -Scope Process Bypass -Force; .\build_quake.ps1
```

产物在 `manual-build-quake/`：
`quake_earthquake.hex`（烧录用，IHEX）、`quake_earthquake.bin`（原始二进制）、
`quake_earthquake.elf`/`.map`（调试/反查）。

**烧录（芯片 STM32F103ZETx，板载 ST-Link）：**

```powershell
STM32_Programmer_CLI -c port=SWD -w manual-build-quake\quake_earthquake.hex -v -rst
```

也可在 Keil/STM32CubeProgrammer 图形界面里 `manual-build-quake\quake_earthquake.hex` 直接下载。

> 说明：`build_quake.ps1` 里的 `$toolRoot` 指向本机 STM32CubeIDE 1.16.0 的内置工具链路径；
> 换机器/版本时改这一处即可。工程也可直接用 STM32CubeIDE「导入现有工程」打开（Core+Drivers）。

## 目录结构

```
地震救援小车/
├── Core/                   # 平台层（HAL 配置、gpio、hal_msp、中断 stm32f1xx_it.c、启动、链接脚本）
├── Drivers/                # STM32F1 HAL + CMSIS（板级库）
├── motion/                 # 【原样拷贝，勿改】实验七的循迹+避障+驱动控制代码
│   ├── line_tracking.c/.h        # 四路循迹闭环
│   ├── line_recovery.c/.h        # 丢线搜索
│   ├── line_obstacle_bypass.c/.h # 黑线绕障控制器（红外+超声波触发，绕障核心）
│   ├── line_bypass_travel.c/.h   # 绕障直线段
│   ├── line_bypass_turn.c/.h     # 绕障转向段
│   ├── line_sensor_sample.c/.h   # 循迹传感器采样
│   ├── line_sensor_clock.c       # 覆盖 HAL_IncTick，在 SysTick 里采样
│   ├── line_wait_guard.c/.h      # 卡死看门（超时自动丢线搜索）
│   ├── line_fault_log.c/.h       # 故障日志
│   ├── line_turn_load.c/.h       # 转向负载
│   ├── line_search_model.h       # 搜索模型
│   ├── ultrasonic.c/.h           # 四针超声波驱动
│   ├── ultrasonic_avoid.c/.h     # 超声波避障状态机
│   ├── ultrasonic_motion.c/.h    # 相对运动估计
│   ├── ir_avoid.c/.h             # 双路红外避障
│   ├── drive_base.c/.h           # 四轮驱动闭环
│   ├── motor.c / motorPWM.c/.h   # 电机 PWM
│   ├── motion_advanced.c/.h      # 差速/原地转向
│   ├── wheel_encoder.c/.h        # 编码器
│   ├── wheel_speed_control.c/.h  # 轮速闭环
│   ├── wheel_speed_observer.c/.h # 轮速观测
│   ├── battery_monitor.c/.h      # 电压补偿
│   ├── diagnostic_uart.c/.h      # 调试串口
│   ├── vehicle_geometry.h        # 车体几何
│   ├── mpu6050_bus.c             # MPU6050 软件 I2C 位带总线
│   ├── mpu6050_yaw.c/.h          # 陀螺仪姿态 + 偏航角（上电 2s 标定）
│   ├── gyro_turn.c/.h            # 陀螺仪辅助转向（绕障转向用，掉线回编码器）
│   ├── dfplayer_mini.c/.h        # 扬声器 DFPlayer（UART4，SD 卡 mp3）
│   ├── dfplayer_protocol.c/.h    # DFPlayer 协议
│   ├── buzzer_phrase_40077493715.c/.h # PG12 蜂鸣短语（非阻塞节奏）
│   └── audio_resume_store.c/.h   # 断点续播（Flash 持久化曲目）
│
├── quake/                  # 【新增，可改】地震救援任务代码（STM32 端）
│   ├── quake_config.h            # 任务参数 + 标记/模式编号 + 曲目/翻车参数
│   ├── vision_task.c/.h          # USART2 接收并解析 K210 $T 帧
│   ├── quake_task.c/.h           # 救援任务状态机（含音频事件）
│   └── quake_main.c              # 新主程序（替换实验七 main.c）
│
├── k210/                   # 【新增】K210 识别端（CanMV/MaixPy，拷到 K210 SD 卡跑）
│   ├── main.py                    # 入口：采集 + 模式门控 + 上报 + 显示
│   ├── config.py                  # 阈值/模型路径/编号/时序/串口（唯一需要调的）
│   ├── task_engine.py             # 模式门控 + 去抖 + 自动切换（纯逻辑）
│   ├── ai_detector.py             # 人检测(VOC20)（数字识别方案已废弃移除）
│   ├── detector.py                # 颜色标记（蓝/橙/黑/绿/红/黄）
│   ├── uart_link.py               # 串口协议帧（坐标已钳位，防丢帧）
│   ├── camera.py / display.py     # 摄像头 + LCD 叠加显示
│   └── button.py                  # BOOT 键（短按叠加层/长按切模式）
│
├── build_quake.ps1          # 一键编译（GNU ARM），产物在 manual-build-quake/
├── STM32F103ZETX_FLASH.ld    # 链接脚本（FLASH 508K / RAM 64K）
├── manual-build-quake/       # 构建产物（只保留 hex/bin/elf/map；中间 .o/.d/.su 已清理）
│
├── 素材/                    # 【素材】打印页与图片（不参与编译）
│   ├── 打印素材_输出/             # 6cm 色卡圆 A4 打印页（PNG + PDF）
│   ├── 图片素材/                  # 人像照片、PSD 源文件
│   └── 打印素材_救援彩标.html      # 打印页预览
│
├── tools/                   # 【辅助脚本】不参与固件编译，独立运行
│   ├── 色卡标定.py                # K210 端 LAB 阈值自动标定（6 色，输出可粘回 config.py）
│   ├── render_print_sheet.ps1     # 生成 A4@300dpi 打印页 PNG
│   ├── build_pdf.ps1              # 4 页 PNG 合并为一个 A4 多页 PDF
│   └── build_pdf_pages.ps1        # 每页各导出一个单页 PDF
│
├── _archive/                # 【归档】已废弃的方案
│   └── 数字模型测试.py             # 旧数字识别方案的调试脚本（已弃用）
└── README.md
```

> `tools/` 里的 `.ps1` 都用 `Split-Path -Parent $PSScriptRoot` 找工程根，因此移进子目录后仍能正确定位
> `素材/`。`build_quake.ps1` **必须留在根目录**（它用 `$PSScriptRoot` 作为工程根）。

> 中断文件 `Core/Src/stm32f1xx_it.c` 用的是地震版（USART2→视觉、UART4→DFPlayer、
> EXT15_10→超声波），即原 `quake/platform/stm32f1xx_it.c`，为避免重复已并入 Core/Src。

## 识别与标记（K210 ↔ STM32）

K210 端（`k210/`，拷到 K210 SD 卡运行）通过 USART2 发：

```
$T,<mode>,<marker>,<cx>,<cy>,<area>#
```

| 标记 marker | 含义 | K210 识别方式 | STM32 动作 |
|---|---|---|---|
| 1 | 救人任务 | **橙色圆** | 派发救人、开始巡逻（此后 15s 不识别任务牌/蓝牌） |
| 2 | 送物资任务 | **黑色圆** | 派发送物资、开始巡逻（同上） |
| 3 | 人 | VOC20 person | **立即停车 + 蜂鸣 2s + 播报（不做多帧确认）** |
| 4 | 物资点 | 颜色绿 | 停车 + 多帧确认 → 投放播报 |
| 5 | 危险区 | 颜色红 | 停车避险（纯循迹，不避障） |
| 6 | 余震预警 | 颜色黄 | 停车避险 |
| 7 | 复位牌 | 颜色蓝 | 静默回「等待派发」（停车，不播报） |
| -1 | 无 | — | 无动作 |

> 编号定义在 `quake/quake_config.h`，必须和 K210 端 `config.py` 的 `MARKER_*` 一一对应。

### K210 端可用性与演示提醒

1. **先按 KEY1 再亮颜色圆**：K210 初始 `MODE_TASK`，一旦看到橙色/黑色圆就**自动切到救人/送物资**、
   不再发派发标记。KEY1 让 STM32 进入「等待派发」（车不动），看到任务圆后才开始巡逻；
   所以务必**先按 KEY1、再把任务圆放到画面**，否则任务会丢失。
   **派发后 15 秒内 STM32 不识别任务牌和复位牌**，避免牌还留在画面里造成重复识别。
2. **只调 `k210/config.py`** 即可适配环境：6 组 LAB 颜色阈值、人的模型路径与分数阈值；环境光不稳把
   `camera.py` 的 `set_auto_gain/auto_whitebal` 改回 True。
3. **颜色圆要点**：打印页上的圆是直径 6cm；演示时离镜头近、居中，并落在画面**中间横带**
   （`config.ROI` = 全宽 × 第 40~200 行）内。六个阈值都是 `tools/色卡标定.py` 在本机灯光下
   实测标定的，比教程默认值可靠得多。
4. **已做可靠性处理**：`uart_link.py` 会把 `cx/cy/area` **钳到 STM32 合法范围**——VOC20 人检测的
   坐标来自 320×256 网络空间，不钳位可能因 `cy>239/`cx>319` 被 STM32 整帧丢弃。
5. 模型放 K210 SD 卡：现在**只需要** `/sd/KPU/voc20_object_detect/voc20_detect.kmodel`
   （数字模型 `digit_imgs_model/` 已弃用，留着不影响）；接线 K210 P8(TX)→STM32 PD6、P6(RX)→PD5。
6. **识别确认分两类**：
   - **人（marker 3）**：**立即触发**——第一帧就停车 + 蜂鸣 2s + 播报，不做多帧确认。
   - **其它标记（物资点/危险/预警/任务牌）**：K210 端 `CONFIRM_FRAMES=1`（第一帧就上报），
     STM32 收到第一帧停车进入 CONFIRMING，连续观察 `QUAKE_MARKER_CONFIRM_FRAMES`（默认 4 帧，约 200ms），
     一直是同一标记才确认进入动作，中途有帧没识别到就放弃、恢复巡逻；另有 `QUAKE_CONFIRM_TIMEOUT_MS` 兜底。
   - **任务牌（橙/黑）与复位牌（蓝）在 K210 端要连续识别 `TASK_TRANSITION_MS` / `RESET_TRANSITION_MS`（各 300ms）
     才切模式**，期间持续上报 marker 1/2/7，让 STM32 凑够确认帧（否则第一帧就切走、STM32 永远确认不了）。
   - **复位牌（蓝）在 STM32 端不确认**：收到 marker 7 就静默回「等待派发」。

## 状态机（quake_task.c）

```
IDLE(上电停车) --KEY1--> WAIT_TASK(停车等派发，不动)
WAIT_TASK + 橙色圆(救人)   -> 派发救人，进 PATROL(纯循迹巡逻)；此后 15s 不识别任务牌/蓝牌
WAIT_TASK + 黑色圆(送物资) -> 派发送物资，进 PATROL（同上）
PATROL + 人(任务=救人)     -> 立即 ACTION(停车 + 蜂鸣 2s + 播报)，不做多帧确认
PATROL + 物资点(任务=送物资) -> CONFIRMING(停车，4 帧确认) -> ACTION(投放播报)
PATROL + 危险/预警         -> CONFIRMING -> ALERT(停车避险，语音播完回巡逻)
PATROL + 复位牌(蓝, 15s真空外) -> WAIT_TASK(清任务，静默回等派发，停车)
ACTION 语音播完            -> 回 PATROL 继续走（清任务，7s 不重复识别人/物资/危险）
ALERT 语音播完             -> 回 PATROL 继续走
KEY2                     -> STOPPED(全局停车)
```

- `QuakeTask_IsMotionLocked()` 为真时，主循环**只停车、不循迹**；
- 否则主循环照常跑 `motion/` 的循迹 + 避障（见下节「循迹避障 = 实验七模式一」）。
- 每次状态切换还会产生一个 `QuakeAudioEvent`，由 `quake_main.c` 映射到扬声器曲目播报（见「扬声器播报」）。

## 巡逻 = 纯四路循迹（避障/绕障已关闭）

巡逻（PATROL）时，`quake_main.c` 只跑实验七模式一的**四路循迹** `line_tracking`，
**不开启红外/超声波绕障**（`avoidance_enabled` 恒为 0）。识别到红色危险只触发停车避险（ALERT），
不再 10 秒后开启绕障。

> 避障/绕障代码仍保留在 `motion/`（`ir_avoid`、`ultrasonic_avoid`、`line_obstacle_bypass` 等），
> 但主循环不再调用；如需恢复绕障，把 `quake_main.c` 里 `avoidance_enabled = 0` 的逻辑改回
> 「识别到红色危险后延时开启」即可（`quake_config.h` 的 `QUAKE_AVOID_ARM_DELAY_MS` 已标记停用）。

> 陀螺仪 `gyro_turn` / `MpuYaw_Task()` 仍保留（翻车检测用），上电后静置 2 秒完成零偏标定。

## 扬声器播报（DFPlayer + SD 卡，11 首语音）

`quake_main.c` 用一套**单曲自动排队/切换**管理 DFPlayer：任务搜索时**循环广播**，
事件触发**一次性播报**打断广播，播完再自动恢复。曲目号在 `quake_config.h`：

| 曲目 | 情境 | 语音内容 | 播放 | 触发 |
|---|---|---|---|---|
| 0001.mp3 | 出发 | 「救援巡逻启动」 | 一次性 | 按 KEY1 开始巡逻 |
| 0002.mp3 | 救人任务 | 「收到救人任务」 | 一次性 | 识别到橙色圆 |
| 0003.mp3 | 送物资任务 | 「收到送物资任务」 | 一次性 | 识别到黑色圆 |
| 0004.mp3 | 救人 | 「发现被困人员，开始救援」 | 一次性 | 识别到人 |
| 0005.mp3 | 投放 | 「物资投放完成」 | 一次性 | 到物资点 |
| 0006.mp3 | 危险 | 「危险，紧急避险」 | 一次性 | 危险区 / 余震 / 翻车 |
| 0007.mp3 | 复位 | 「任务完成，返回待命」 | （已停用） | 蓝牌现静默，不播报 |
| 0008.mp3 | 救人·搜索广播 | 「这里是救援队，正在搜寻…」 | **循环** | 救人巡逻中 |
| 0009.mp3 | 救人·已定位 | 「已发现您的位置，请保持不动…」 | 一次性 | 识别到人 |
| 0010.mp3 | 送物资·运输广播 | 「这里是救援物资车，正在运送物资…」 | **循环** | 送物资巡逻中 |
| 0011.mp3 | 送物资·物资发放 | 「物资已送到，请按顺序领取…」 | 一次性 | 到物资点 |

- **救人** = 8（巡逻循环）+ 9（已定位），**送物资** = 10（巡逻循环）+ 11（发放）。
- 巡逻且有任务时，救人任务循环播 0008、送物资任务循环播 0010；事件语音一次性打断，
  播完自动恢复对应循环；识别到人/到物资点时会先播 0004/0005（通报）再播 0009/0011（安抚）。
- 巡逻（PATROL）和停车确认（CONFIRMING）时保持对应任务循环广播；待命（WAIT_TASK）、
  停车（IDLE/STOPPED）、避险（ALERT）、驱动故障时**停止循环广播**（安全优先）。
- DFPlayer 用 **UART4（9600 8N1）**：PC10/TX → DFPlayer RX（串 680Ω~1kΩ 降压）、
  PC11/RX ← DFPlayer TX、5V/GND 接板子 J8（见实验七接线）。

### 语音生成建议（SD 卡还没有音频）

上面 11 句中文语音可用任意 TTS 工具合成后转成 mp3（16kHz/32kbps 即可，循环广播 8~14 秒、
一次性 3~4 秒）。建议选**吐字清晰、语速中等**的男/女声，导出后按上表重命名
`0001.mp3 ~ 0011.mp3` 放进 SD 卡根目录 `mp3/`。

> 合成后先在本机听一遍再放 SD 卡；DFPlayer 只认 FAT16/FAT32 + `mp3/000N.mp3` 命名。

## 陀螺仪姿态（MPU6050）

- MPU6050 插板上专用 IMU 座，用**软件 I2C（PB10=SCL / PB11=SDA，AD0=PE0）**位带读写，
  不占硬件 I2C、不用中断/定时器，主循环每圈 `MpuYaw_Task(now, stationary)` 即可。
- 两个用途：
  1. **绕障转向辅助**：`line_bypass_turn` 用偏航角闭环转向，比编码器更抗打滑；
  2. **翻车/大倾角检测**（`quake_main.c::quake_is_tilted()`）：读加速度计，
     Z 轴重力分量 < 0.5g（约 60° 倾角）或 X/Y 侧向 > 0.87g 时判为翻车，
     立即停车 + 播放危险警报曲目，扶正后自动恢复巡逻。
- 阈值在 `quake_config.h` 的 `QUAKE_TILT_Z_LSB / QUAKE_TILT_XY_LSB`（1g=16384 LSB）。

## 集成到 CubeIDE（推荐：复制实验七工程再裁剪）

1. **复制** `test-exp7-unified-motion-v1` 整个工程为一个新工程（例如 `QuakeRescue`）。
2. 把本目录 `motion/` 的 `.c` 放 `Core/Src`、`.h` 放 `Core/Inc`（这些文件本来就来自实验七，内容一致；想保险起见覆盖一遍即可）。
3. 把 `quake/` 里的 `quake_config.h`、`vision_task.*`、`quake_task.*` 放进工程；把 `quake_main.c` 作为**唯一含 `main()` 的文件**（删除或排除实验七的 `main.c`）。
4. `Core/Src/stm32f1xx_it.c` **已是地震版**（USART2→视觉帧、UART4→DFPlayer、EXTI15_10→超声波），
   不要再用实验七的原版覆盖它；原 `quake/platform/` 目录已并入 `Core/Src`，无需单独拷贝。
5. 删除不再需要的实验模块（防止重复定义/占用外设）：
   - 遥控：`ir_remote`、`ir_remote_keymap`
   - OLED：`oled_status`
   - 视觉旧实验：`vision_uart`、`vision_detection`、`vision_detection_parser`、`vision_line_v4*`
   - 标志导航：`sign_route`、`sign_route_config`、`sign_slowdown`、`simple_line_mode`
   - 其它实验：`square_encoder`、`figure8_encoder`、`encoder_linear`、`encoder_straight`、`encoder_turn`、`line_tracking_lift_test`
6. 编译烧录。K210 8脚(TX)→STM32 PD6(RX)、6脚(RX)→STM32 PD5(TX)、共地，115200。

> 保留的平台文件（实验七原样）：`main.h`、`gpio.c/.h`、`stm32f1xx_it.h`、`stm32f1xx_hal_msp.c`、`system_stm32f1xx.c`、`startup`、`Drivers/`、`.ioc`。

> **本版保留（不要删）**：扬声器 `dfplayer_mini`、`dfplayer_protocol`、`buzzer_phrase_40077493715`、`audio_resume_store`，
> 以及陀螺仪 `mpu6050_bus`、`mpu6050_yaw`、`gyro_turn`——都由 `quake_main.c` / 绕障 / `stm32f1xx_it.c` 用到。

## 演示流程（录视频建议）

1. 上电：车停住，`IDLE`；**上电后静置 2 秒**让陀螺仪标定零偏。
2. 按 KEY1：进入「等待派发」（车不动），播报「救援巡逻启动」。
3. 拿**橙色圆**给 K210：派发救人任务、开始巡逻，播报「收到救人任务」，
   随后巡逻中**循环播放「救人·搜索广播」0008**。
4. 车巡线遇到「人」（VOC20 识别）：**立即停车 + 蜂鸣 2s + 播报「发现被困人员，开始救援」0004 →
   「已发现您的位置」0009**；播完车继续往前走（任务已清空，同一人 7s 内不再重复触发）。
5. 换任务：先拿**蓝复位牌**把 K210 切回 TASK、车静默回到「等待派发」停车（不播报）；
   再拿**黑色圆** → 送物资任务，播报「收到送物资任务」，巡逻中**循环播放「送物资·运输广播」0010**，
   巡线到「绿物资点」→ 停车确认 → **播报「物资投放完成」0005 →「物资已送到」0011**，播完继续走。
6. 途中拿「红 × / 黄警示牌」→ **停车避险 + 播报「危险，紧急避险」0006**；播完车继续走（7s 内不再重复触发）。
7. 拿「蓝复位牌」→ **静默回「等待派发」停车**（清任务、不播报、不做多帧确认）。
8. 演示「翻车保护」：把车侧倾/翻倒 → **立即停车 + 播报「危险，紧急避险」0006**，扶正后自动恢复巡逻。
9. 按 KEY2 随时停车。

## 参数调整（只改 quake_config.h）

| 参数 | 含义 | 默认 |
|---|---|---|
| QUAKE_LINE_SPEED | 循迹基础速度（PWM，0~3599） | 3000 |
| QUAKE_ACTION_MAX_MS | 救人/投放「语音未播完」的兜底超时（正常是语音播完就走） | 15000 ms |
| QUAKE_ACTION_BEEP_MS | 识别到人/物资点后的蜂鸣时长 | 2000 ms |
| QUAKE_VACUUM_MS | 危险/人员/物资处理完后的不重复识别真空期 | 7000 ms |
| QUAKE_TASK_VACUUM_MS | 派发任务牌后不识别任务牌/复位牌的真空期 | 15000 ms |
| QUAKE_MARKER_CONFIRM_FRAMES | 物资点/危险/预警/任务牌停车确认所需帧数（人不用确认） | 4 |
| QUAKE_CONFIRM_TIMEOUT_MS | 停车确认兜底超时（K210 断连时恢复巡逻） | 2000 ms |
| RESET_TRANSITION_MS（K210） | 复位牌(蓝)连续识别多久才切回 TASK，期间持续上报 marker 7 | 300 ms |
| TASK_TRANSITION_MS（K210） | 任务牌(橙/黑)连续识别多久才切模式，期间持续上报 marker 1/2 供 STM32 确认 | 300 ms |
| QUAKE_RGB_FLASH_MS | RGB 报警灯闪烁半周期 | 250 ms |
| QUAKE_AUDIO_TRACK_* | 各事件/广播对应 DFPlayer 曲目号（START/任务/救人/投放/危险 + 搜索&运输广播 + 定位/发放） | 1~11 |
| QUAKE_TILT_Z_LSB / QUAKE_TILT_XY_LSB | 翻车判定阈值（1g=16384 LSB） | 8000 / 14000 |

> 避障相关参数（`QUAKE_ULTRASONIC_*`、`QUAKE_BYPASS_*`、`QUAKE_AVOID_ARM_DELAY_MS`）仍保留在
> `quake_config.h`，但因避障已关闭不再使用。

蜂鸣（PG12）为**高电平有效**，`quake_main.c::quake_beep()` 直接驱动；扬声器（DFPlayer）音量已设为**最大 30**、
曲目见 `motion/dfplayer_mini.c`（`DFPLAYER_MINI_DEFAULT_VOLUME`）。
