/*
 * 地震灾后救援小车 —— 全局配置（新增代码）
 *
 * 只改本文件即可调整：任务速度、避障距离、动作停留时长、
 * 以及和 K210 端 config.py 一致的标记/模式编号。
 */

#ifndef QUAKE_CONFIG_H
#define QUAKE_CONFIG_H

#include <stdint.h>

/* ---------- K210 上报的标记 ID（必须与 K210 端 config.py 的 MARKER_* 一致） ---------- */
#define QUAKE_MARKER_NONE          (-1)
#define QUAKE_MARKER_TASK_RESCUE     1   /* 橙色圆 -> 救人任务 */
#define QUAKE_MARKER_TASK_DELIVER    2   /* 紫色圆 -> 送物资任务 */
#define QUAKE_MARKER_PERSON          3   /* 人（VOC20） */
#define QUAKE_MARKER_SUPPLY          4   /* 物资点（绿） */
#define QUAKE_MARKER_DANGER          5   /* 危险区（红） */
#define QUAKE_MARKER_WARNING         6   /* 余震预警（黄） */
#define QUAKE_MARKER_RESET           7   /* 复位牌（蓝） */

/* 「识别到第一帧就停车、若干帧后确认」的确认帧数。
   K210 端 CONFIRM_FRAMES=1（识别到第一帧就上报），STM32 收到后立即停车。
   在 QUAKE_MARKER_CONFIRM_WINDOW_FRAMES 帧的观察窗口内，命中（与第一帧同一标记）
   QUAKE_MARKER_CONFIRM_FRAMES 次就确认进入对应动作；窗口内没凑够就放弃并恢复巡逻。
   允许中间有几帧没识别到（K210 帧率低/检测抖动时更抗丢帧）。 */
#define QUAKE_MARKER_CONFIRM_FRAMES   4U
/* 观察窗口帧数：最多看这么多帧，期间命中确认帧数就确认，否则放弃恢复巡逻。 */
#define QUAKE_MARKER_CONFIRM_WINDOW_FRAMES  8U
/* 停车确认的最长等待时间：若 K210 中途不再上报（断连/异常），超过该时长
   仍没确认就恢复巡逻，避免一直停在原地。 */
#define QUAKE_CONFIRM_TIMEOUT_MS    2000U

/* ---------- K210 上报的模式 ID ---------- */
#define QUAKE_MODE_NONE      0
#define QUAKE_MODE_TASK      1
#define QUAKE_MODE_RESCUE    2
#define QUAKE_MODE_DELIVER   3

/* ---------- 循迹基础速度（PWM，周期 3599） ---------- */
#define QUAKE_LINE_SPEED             3000

/* ---------- 超声波避障参数（沿用实验七综合模式的数值） ---------- */
#define QUAKE_ULTRASONIC_CRUISE_SPEED   3599
#define QUAKE_ULTRASONIC_SLOW_SPEED     2600
/* 灵敏度已下调：STOP/CLEAR/EMERGENCY 距离都比默认（10/18/16）更小，
   只有障碍真正贴近时才刹车，减少远距离/地面回声误触发。
   若仍嫌太灵敏，可继续把 STOP_CM 往 6 调；反之往上调回 10。 */
#define QUAKE_ULTRASONIC_STOP_CM           8U
#define QUAKE_ULTRASONIC_CLEAR_CM         15U
#define QUAKE_ULTRASONIC_EMERGENCY_MAX_CM 13U
#define QUAKE_ULTRASONIC_TURN_INNER     2800
#define QUAKE_ULTRASONIC_TURN_OUTER     3300
#define QUAKE_ULTRASONIC_TURN_TIME_MS    500U
#define QUAKE_ULTRASONIC_REVERSE_SPEED  2600
#define QUAKE_ULTRASONIC_STOP_TIME_MS    120U
#define QUAKE_ULTRASONIC_REVERSE_TIME_MS 300U
#define QUAKE_ULTRASONIC_GUARD_TIME_MS    60U
#define QUAKE_ULTRASONIC_NO_ECHO_COUNT     3U

/* ---------- 超声波「速度自适应紧急制动」参数（实验七模式一） ----------
 * 接近障碍时按当前轮速计算安全刹车距离（越靠近越敏感），
 * 上限 QUAKE_ULTRASONIC_EMERGENCY_MAX_CM、下限 STOP_CM。 */
#define QUAKE_ULTRASONIC_LOOKAHEAD_MS     70U
#define QUAKE_ASSUMED_FAST_SPEED_CPS    5300U   /* 测速窗口未就绪时按此估计 */
#define QUAKE_EMERGENCY_BRAKE_SPEED_CPS 3500U
#define QUAKE_FAST_SPEED_HOLD_MS          220U
#define QUAKE_ENCODER_COUNTS_PER_REV    1040U
#define QUAKE_WHEEL_DIAMETER_MM           47U
#define QUAKE_PI_X10000                 31416U

/* ---------- V2 黑线绕障控制器（实验七模式一，红外+超声波一起触发） ---------- */
#define QUAKE_BYPASS_REVERSE_CPS        1900U
#define QUAKE_BYPASS_FORWARD_CPS        2600U
#define QUAKE_BYPASS_CLEAR_PROBE_CPS    2200U
#define QUAKE_BYPASS_RETURN_CPS         2300U
#define QUAKE_BYPASS_TURN_CPS           2500U

/* ---------- 绕障重新布防 + 红外触发去抖（实验七模式一） ---------- */
#define QUAKE_BYPASS_REARM_DELAY_MS     1000U   /* 绕障完成后多久才允许再触发 */
#define QUAKE_BYPASS_REARM_CLEAR_SAMPLES   10U   /* 连续多少次红外无遮挡才算离开障碍 */
#define QUAKE_BYPASS_IR_TRIGGER_CONFIRM_MS 30U   /* 红外方向需保持该时长才确认触发 */

/* ---------- 地震任务动作时序 ----------
 * 救人/投放：停车后播完相关语音（发现人员/投放完成 + 后续安抚/指引）再出发；
 * QUAKE_ACTION_MAX_MS 只是「语音一直没报播完」的兜底超时。 */
#define QUAKE_ACTION_MAX_MS        15000U

/* ---------- 识别到人/物资点后的蜂鸣时长 ----------
 * 动作开始后蜂鸣只响这么久提示一下，之后静音，语音交给扬声器播报。 */
#define QUAKE_ACTION_BEEP_MS        2000U

/* ---------- 识别后真空期 ----------
 * 危险/预警、人员、物资点识别一次并处理完后，接下来 QUAKE_VACUUM_MS 内
 * 不再重复识别同一类标记，让小车能继续往前开（不会被同一张牌一直挡停）。 */
#define QUAKE_VACUUM_MS             7000U

/* ---------- 任务牌派发后的真空期 ----------
 * 识别到任务牌（橙/紫）并派发后，接下来 QUAKE_TASK_VACUUM_MS 内
 * 不再识别任务牌和复位牌（蓝），避免牌还留在画面里/反射导致重复识别。 */
#define QUAKE_TASK_VACUUM_MS       15000U

/* ---------- 避障开启时机（已停用） ----------
 * 避障已整体关闭：红色危险只停车报警、不再开启避障。
 * QUAKE_AVOID_ARM_DELAY_MS 保留仅为兼容，不再被主循环使用。 */
#define QUAKE_AVOID_ARM_DELAY_MS   10000U

/* ---------- RGB 报警灯 ----------
 * 巡逻 = 白光；危险（红）闪烁、预警（黄）闪烁、发现人员（蓝）闪烁、
 * 物资点（绿）闪烁；翻车 = 常亮红。QUAKE_RGB_FLASH_MS 是闪烁半周期。 */
#define QUAKE_RGB_FLASH_MS           250U

/* ---------- 扬声器 DFPlayer 曲目映射（SD 卡 /mp3/NNNN.mp3） ----------
 * 11 首：0001~0007 一次性事件播报；0008/0010 为任务搜索循环广播；
 * 救人=8(循环)+9(已定位)，送物资=10(循环)+11(发放)。 */
#define QUAKE_AUDIO_TRACK_START              1U   /* 救援巡逻启动 */
#define QUAKE_AUDIO_TRACK_TASK_RESCUE        2U   /* 收到救人任务 */
#define QUAKE_AUDIO_TRACK_TASK_DELIVER       3U   /* 收到送物资任务 */
#define QUAKE_AUDIO_TRACK_RESCUE_FOUND       4U   /* 发现被困人员，开始救援 */
#define QUAKE_AUDIO_TRACK_DELIVER_DONE       5U   /* 物资投放完成 */
#define QUAKE_AUDIO_TRACK_DANGER             6U   /* 危险，紧急避险 */
#define QUAKE_AUDIO_TRACK_RESET              7U   /* 任务完成，返回待命 */
#define QUAKE_AUDIO_TRACK_RESCUE_SEARCH      8U   /* 救人·搜索广播（循环） */
#define QUAKE_AUDIO_TRACK_RESCUE_LOCATED     9U   /* 救人·已定位 */
#define QUAKE_AUDIO_TRACK_DELIVER_TRANSPORT  10U  /* 送物资·运输广播（循环） */
#define QUAKE_AUDIO_TRACK_DELIVER_DISTRIBUTE 11U  /* 送物资·物资发放 */

/* ---------- 陀螺仪（MPU6050）姿态：翻车/大倾角检测 ----------
 * 加速度计 ±2g，1g = 16384 LSB。正常水平时 Z≈+16384、X/Y≈0。
 * Z 分量 < 0.5g（约 60° 倾角）或 X/Y 侧向分量 > 0.87g（侧翻）判为翻车，
 * 触发停车 + 危险警报。 */
#define QUAKE_TILT_Z_LSB           8000
#define QUAKE_TILT_XY_LSB         14000

#endif /* QUAKE_CONFIG_H */
