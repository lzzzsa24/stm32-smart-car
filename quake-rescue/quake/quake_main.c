/*
 * 地震灾后救援小车 —— 主程序（新增代码，替换实验七的 main.c）
 *
 * 目标芯片：STM32F103ZETx（YB-DSF01-V1.1）
 *
 * 运行逻辑：
 *   - 上电锁存停车（IDLE）；按 KEY1 进入巡逻（PATROL）。
 *   - 巡逻 = 实验七「模式一（KEY1 综合模式）」的完整循迹 + 避障：
 *       四路循迹（line_tracking）+ 红外（ir_avoid）+ 超声波（ultrasonic_avoid）
 *       -> 红外和超声波一起触发 V2 黑线绕障控制器（line_obstacle_bypass），
 *          绕障转向由陀螺仪（mpu6050_yaw + gyro_turn）辅助，找不到线时
 *          由卡死看门（line_wait_guard + line_recovery）自动丢线搜索，
 *          故障写入 line_fault_log。
 *   - 收到 K210 的 $T 帧后由 quake_task 决策：
 *       橙色圆 -> 救人任务；紫色圆 -> 送物资任务；
 *       识别人 -> 停车救人；物资点 -> 停车投放（语音播完再出发）；
 *       危险/预警 -> 停车避险（红/黄 RGB 灯 + 蜂鸣声光报警，语音播完再出发）；
 *       复位牌（蓝）-> 停车等待重新派发任务（橙/紫）。
 *   - 扬声器（DFPlayer，UART4）按事件播报：派发任务后循环播放对应搜索/运输
 *     广播，发现人/物资点/危险则插播相关语音；陀螺仪检测翻车/大倾角停车避险；
 *     危险区/余震预警/翻车时 RGB 灯（红/黄）+ 蜂鸣做声光报警。
 *   - KEY2 随时全局停车。
 *
 * 注意：本文件只有 main() 一个入口，编译时不要再链接实验七的 main.c。
 */

#include "main.h"
#include "gpio.h"

#include "battery_monitor.h"
#include "buzzer_phrase_40077493715.h"
#include "dfplayer_mini.h"
#include "diagnostic_uart.h"
#include "drive_base.h"
#include "gyro_turn.h"
#include "ir_avoid.h"
#include "line_bypass_turn.h"
#include "line_fault_log.h"
#include "line_obstacle_bypass.h"
#include "line_recovery.h"
#include "line_sensor_sample.h"
#include "line_tracking.h"
#include "line_wait_guard.h"
#include "motorPWM.h"
#include "motion_advanced.h"
#include "mpu6050_yaw.h"
#include "ultrasonic.h"
#include "ultrasonic_avoid.h"
#include "ultrasonic_motion.h"
#include "wheel_encoder.h"
#include "wheel_speed_control.h"
#include "wheel_speed_observer.h"

#include "quake_config.h"
#include "quake_task.h"
#include "vision_task.h"

void SystemClock_Config(void);
void Error_Handler(void);

/* ---------- 运行期状态（沿用实验七模式一的同名变量） ---------- */
static int16_t ultrasonic_forward_speed_limit;
static uint32_t last_fast_speed_cps;
static uint32_t last_fast_speed_ms;
static int8_t next_bypass_direction;
static uint8_t bypass_rearm_pending;
static uint8_t bypass_ir_clear_samples;
static uint32_t bypass_rearm_not_before_ms;
static uint8_t bypass_ir_trigger_candidate;
static uint32_t bypass_ir_trigger_since_ms;
static uint32_t bypass_front_trigger_ms;
static uint32_t bypass_front_sample_ms;
static uint8_t bypass_front_obstacle;
static LineWaitGuard line_wait_guard;
static int8_t line_wait_side;
static LineObstacleBypassConfig bypass_config;
static QuakeState prev_quake_state;
static uint8_t tilt_active;
static uint32_t action_beep_until_ms;

/* ---------- 避障模式开关 ----------
 * 已整体关闭：始终纯循迹，识别到红色也只停车报警，不再开启避障。 */
static uint8_t avoidance_enabled;
static uint8_t avoidance_armed;
static uint32_t avoidance_arm_at_ms;

/* ---------- 回绕安全的延时判断 ---------- */
static uint8_t tick_reached(uint32_t now, uint32_t deadline)
{
  return (int32_t)(now - deadline) >= 0 ? 1U : 0U;
}

/* ---------- 超声波避障回调（沿用实验七的做法） ---------- */
static void app_drive_forward(int16_t left_speed, int16_t right_speed)
{
  int16_t limit = left_speed < right_speed ? left_speed : right_speed;

  if (limit < 0)
  {
    limit = 0;
  }
  /* 前进回调只发布速度上限，真正的转向/前进由下方循迹执行。 */
  ultrasonic_forward_speed_limit = limit;
}

static void app_stop(void)
{
  ultrasonic_forward_speed_limit = 0;
  DriveBase_Stop(DRIVE_STOP_BRAKE);
}

static void app_drive_backward(int16_t left_speed, int16_t right_speed)
{
  ultrasonic_forward_speed_limit = 0;
  advanced_drive_backward(left_speed, right_speed);
}

static void app_turn_left(int16_t inner_speed, int16_t outer_speed)
{
  (void)inner_speed;
  ultrasonic_forward_speed_limit = 0;
  advanced_spin_left(outer_speed);
}

static void app_turn_right(int16_t inner_speed, int16_t outer_speed)
{
  (void)inner_speed;
  ultrasonic_forward_speed_limit = 0;
  advanced_spin_right(outer_speed);
}

static void configure_ultrasonic_avoid(void)
{
  UltrasonicAvoid_Init(app_drive_forward, app_stop,
                       app_turn_left, app_turn_right);
  UltrasonicAvoid_SetThresholds(QUAKE_ULTRASONIC_STOP_CM,
                                QUAKE_ULTRASONIC_CLEAR_CM);
  UltrasonicAvoid_SetEmergencyDistance(QUAKE_ULTRASONIC_STOP_CM);
  UltrasonicAvoid_SetSpeeds(QUAKE_ULTRASONIC_CRUISE_SPEED,
                            QUAKE_ULTRASONIC_SLOW_SPEED,
                            QUAKE_ULTRASONIC_TURN_INNER,
                            QUAKE_ULTRASONIC_TURN_OUTER);
  UltrasonicAvoid_SetTurnTime(QUAKE_ULTRASONIC_TURN_TIME_MS);
  UltrasonicAvoid_SetEscapeManeuver(app_drive_backward,
                                    QUAKE_ULTRASONIC_REVERSE_SPEED,
                                    QUAKE_ULTRASONIC_STOP_TIME_MS,
                                    QUAKE_ULTRASONIC_REVERSE_TIME_MS,
                                    QUAKE_ULTRASONIC_GUARD_TIME_MS);
  UltrasonicAvoid_SetNoEchoFallback(1U, QUAKE_ULTRASONIC_NO_ECHO_COUNT);
}

/* ---------- 接近障碍时的当前轮速（测速窗口未就绪则按全速估计） ---------- */
static uint32_t app_approach_speed_cps(void)
{
  uint32_t speed_cps = 0U;
  uint32_t now = HAL_GetTick();

  if (WheelSpeedObserver_GetAverageCps(&speed_cps) != 0U)
  {
    if (speed_cps >= QUAKE_EMERGENCY_BRAKE_SPEED_CPS)
    {
      last_fast_speed_cps = speed_cps;
      last_fast_speed_ms = now;
    }
    else if (last_fast_speed_cps != 0U &&
             now - last_fast_speed_ms <= QUAKE_FAST_SPEED_HOLD_MS)
    {
      speed_cps = last_fast_speed_cps;
    }
    else
    {
      last_fast_speed_cps = 0U;
    }
    return speed_cps;
  }

  last_fast_speed_cps = QUAKE_ASSUMED_FAST_SPEED_CPS;
  last_fast_speed_ms = now;
  return QUAKE_ASSUMED_FAST_SPEED_CPS;
}

/* ---------- 速度自适应紧急刹车距离 ---------- */
static uint16_t app_emergency_distance_cm(uint32_t speed_cps)
{
  uint64_t speed_mm_s;
  uint64_t lookahead_cm;
  uint32_t distance_cm;

  speed_mm_s = ((uint64_t)speed_cps * QUAKE_PI_X10000 *
                QUAKE_WHEEL_DIAMETER_MM +
                (uint64_t)QUAKE_ENCODER_COUNTS_PER_REV * 5000ULL) /
               ((uint64_t)QUAKE_ENCODER_COUNTS_PER_REV * 10000ULL);
  lookahead_cm = (speed_mm_s * QUAKE_ULTRASONIC_LOOKAHEAD_MS + 9999ULL) /
                 10000ULL;
  distance_cm = QUAKE_ULTRASONIC_STOP_CM + (uint32_t)lookahead_cm;
  if (distance_cm > QUAKE_ULTRASONIC_EMERGENCY_MAX_CM)
  {
    distance_cm = QUAKE_ULTRASONIC_EMERGENCY_MAX_CM;
  }
  if (distance_cm >= QUAKE_ULTRASONIC_CLEAR_CM)
  {
    distance_cm = QUAKE_ULTRASONIC_CLEAR_CM - 1U;
  }
  return (uint16_t)distance_cm;
}

/* ---------- 红外 + 超声波 -> V2 绕障 ---------- */
static int8_t choose_bypass_direction(const IrAvoidReading *reading)
{
  if (reading->left_obstacle && !reading->right_obstacle)
  {
    return 1;
  }
  if (reading->right_obstacle && !reading->left_obstacle)
  {
    return -1;
  }
  {
    int8_t direction = next_bypass_direction;
    next_bypass_direction = (int8_t)-next_bypass_direction;
    return direction;
  }
}

static uint8_t confirmed_ir_bypass_direction(const IrAvoidReading *reading,
                                              int8_t *direction)
{
  uint8_t mask = (uint8_t)((reading->left_obstacle ? 0x01U : 0U) |
                           (reading->right_obstacle ? 0x02U : 0U));
  uint32_t now = HAL_GetTick();

  if (mask == 0U)
  {
    bypass_ir_trigger_candidate = 0U;
    bypass_ir_trigger_since_ms = now;
    return 0U;
  }
  if (mask != bypass_ir_trigger_candidate)
  {
    bypass_ir_trigger_candidate = mask;
    bypass_ir_trigger_since_ms = now;
    return 0U;
  }
  if (tick_reached(now, bypass_ir_trigger_since_ms +
                        QUAKE_BYPASS_IR_TRIGGER_CONFIRM_MS) == 0U)
  {
    return 0U;
  }

  *direction = choose_bypass_direction(reading);
  bypass_ir_trigger_candidate = 0U;
  bypass_ir_trigger_since_ms = now;
  return 1U;
}

static void make_bypass_input(LineObstacleBypassInput *input,
                              const LineTrackingReading *line,
                              const IrAvoidReading *infrared)
{
  input->line_mask = (uint8_t)((line->x1_black ? 0x01U : 0U) |
                               (line->x2_black ? 0x02U : 0U) |
                               (line->x3_black ? 0x04U : 0U) |
                               (line->x4_black ? 0x08U : 0U));
  input->infrared_valid = ir_avoid_is_enabled() ? 1U : 0U;
  input->front_obstacle = 0U;
  input->left_ir_adc = infrared->left_adc;
  input->right_ir_adc = infrared->right_adc;
  input->left_ir_threshold = ir_avoid_get_left_threshold();
  input->right_ir_threshold = ir_avoid_get_right_threshold();
  input->left_ir_hysteresis = ir_avoid_get_left_hysteresis();
  input->right_ir_hysteresis = ir_avoid_get_right_hysteresis();
}

static uint8_t encoder_fault_beep_code(uint8_t fault_mask)
{
  uint8_t motor;

  for (motor = 0U; motor < 4U; ++motor)
  {
    if ((fault_mask & (uint8_t)(1U << motor)) != 0U)
    {
      return (uint8_t)(motor + 1U);
    }
  }
  if ((fault_mask & 0x10U) != 0U) return 5U;
  if ((fault_mask & 0x20U) != 0U) return 6U;
  if ((fault_mask & 0x40U) != 0U) return 7U;
  if ((fault_mask & 0x80U) != 0U) return 8U;
  return 1U;
}

/* 安全告警蜂鸣仲裁：override=1 会先停掉扬声器/蜂鸣短语再驱动 PG12。 */
static void app_buzzer_safety_write(GPIO_PinState output,
                                    uint8_t safety_override)
{
  if (safety_override != 0U)
  {
    DfPlayerMini_Stop();
    if (BuzzerPhrase400_IsPlaying() != 0U)
    {
      BuzzerPhrase400_Stop();
    }
    HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, output);
  }
  else if (BuzzerPhrase400_IsPlaying() == 0U)
  {
    /* 非 override 模式也要按 output 驱动 PG12：否则传入 GPIO_PIN_SET 的
       告警蜂鸣（ACTION 长响 / ALERT、翻车短哔）会被这里的硬编码 RESET 吞掉，
       导致声光报警的「声」永远不响。蜂鸣短语正在播时则不去抢引脚。 */
    HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, output);
  }
}

/* 循迹显示序位掩码：左外 X2、左内 X1、右内 X3、右外 X4 = 8/4/2/1。 */
static uint8_t line_reading_mask(const LineTrackingReading *line)
{
  if (line == 0)
  {
    return 0U;
  }
  return (uint8_t)((line->x2_black ? 8U : 0U) |
                   (line->x1_black ? 4U : 0U) |
                   (line->x3_black ? 2U : 0U) |
                   (line->x4_black ? 1U : 0U));
}

/* ---------- 卡死看门 + 丢线自动搜索（实验七模式一完整功能） ---------- */
static uint8_t service_quake_line_wait(void)
{
  DriveBaseTelemetry telemetry;
  LineWaitAction action;
  uint32_t now = HAL_GetTick();
  uint8_t paused;

  DriveBase_GetTelemetry(&telemetry);
  paused = telemetry.mode == DRIVE_BASE_STOPPED ||
           telemetry.mode == DRIVE_BASE_BRAKING ||
           telemetry.mode == DRIVE_BASE_FAULT || telemetry.fault_mask != 0U;
  action = LineWaitGuard_Update(&line_wait_guard, 1U, paused, now);

  if (action == LINE_WAIT_RECOVERING)
  {
    LineTrackingReading line = line_tracking_read();
    uint8_t mask = line_reading_mask(&line);

    /* 真实中间接触可提前结束恢复；宽线/横线/单外侧接触不结束恢复。 */
    if ((mask & 6U) && (mask & 9U) != 9U)
    {
      LineWaitGuard_Reset(&line_wait_guard);
      action = LINE_WAIT_END_RECOVERY;
    }
  }

  if (action == LINE_WAIT_BEGIN_RECOVERY)
  {
    LineSearchRecord record = {0};
    MpuYawReading imu;
    LineTrackingReading line = line_tracking_read();
    IrAvoidReading infrared = {0};
    int8_t line_side = line_tracking_direction_evidence(&line);

    line_wait_side = LineRecovery_GetDirection();
    infrared = ir_avoid_read();
    if (infrared.left_obstacle || infrared.right_obstacle)
    {
      line_wait_side = choose_bypass_direction(&infrared);
    }
    else if (line_side)
    {
      line_wait_side = line_side;
    }
    if (line_wait_side == 0)
    {
      line_wait_side = -1;
    }
    record.time_ms = now;
    record.source = LINE_SEARCH_WAIT_RECOVERY;
    record.chosen_side = line_wait_side;
    record.edge_age_ms = record.wide_age_ms = UINT32_MAX;
    record.drive_fault = telemetry.fault_mask;
    record.bypass_fault = LineObstacleBypass_GetFaultMask();
    MpuYaw_GetReading(&imu);
    record.gyro_fault = GyroTurn_GetFault();
    record.imu_fault = imu.fault;
    record.pause_reason = record.drive_fault ? 1U :
        (record.bypass_fault ? 2U :
         (UltrasonicAvoid_GetState() != ULTRASONIC_AVOID_FORWARD ? 3U : 4U));
    LineFaultLog_RecordSearch(&record);
    WheelSpeedObserver_Stop();
    LineObstacleBypass_Stop();
    DriveBase_Stop(DRIVE_STOP_COAST);
    DriveBase_ClearFault();
    LineBypassTurn_Recover();
    DriveBase_SetSpeedLimitCps(0L);
    line_tracking_start_following();
    app_buzzer_safety_write(GPIO_PIN_RESET, 0U);
  }

  if (action == LINE_WAIT_BEGIN_RECOVERY || action == LINE_WAIT_RECOVERING)
  {
    LineWaitGuard_Drive(line_wait_side);
    return 1U;
  }

  if (action == LINE_WAIT_END_RECOVERY)
  {
    DriveBase_Stop(DRIVE_STOP_COAST);
    BuzzerPhrase400_Stop();
    line_tracking_start_following();
    bypass_rearm_pending = 1U;
    bypass_ir_clear_samples = 0U;
    bypass_ir_trigger_candidate = 0U;
    bypass_rearm_not_before_ms = now + QUAKE_BYPASS_REARM_DELAY_MS;
    configure_ultrasonic_avoid();
    WheelSpeedObserver_Start();
    last_fast_speed_cps = 0U;
    last_fast_speed_ms = now;
  }
  return 0U;
}

/* ---------- 陀螺仪姿态：翻车/大倾角检测 ---------- */
static uint8_t quake_is_tilted(void)
{
  MpuYawReading imu;
  int16_t ax;
  int16_t ay;
  int16_t az;

  if (MpuYaw_IsReady(HAL_GetTick()) == 0U)
  {
    return 0U;
  }
  MpuYaw_GetReading(&imu);
  ax = imu.raw_accel[0];
  ay = imu.raw_accel[1];
  az = imu.raw_accel[2];

  if (az < QUAKE_TILT_Z_LSB ||
      ax > QUAKE_TILT_XY_LSB || ax < -QUAKE_TILT_XY_LSB ||
      ay > QUAKE_TILT_XY_LSB || ay < -QUAKE_TILT_XY_LSB)
  {
    return 1U;
  }
  return 0U;
}

/* ---------- 扬声器：事件一次性播报 + 巡逻循环广播（单曲, 自动排队/切换） ----------
 * DFPlayer 一次只放一首。设计：
 *   - 一次性事件语音（0001~0007, 0009, 0011）入队，逐首播完之后停；
 *   - 巡逻中且有任务时，循环播放任务广播（救人=8, 送物资=10），
 *     事件语音优先打断，播完自动恢复对应任务的循环广播；
 *   - 停车/避险/绕障/驱动故障时一律不放循环广播（安全优先）。
 */
#define QUAKE_AUDIO_QUEUE_MAX 6U
static uint16_t quake_audio_queue[QUAKE_AUDIO_QUEUE_MAX];
static uint8_t quake_audio_head;
static uint8_t quake_audio_tail;
static uint8_t quake_audio_count;
static uint8_t quake_audio_oneshot_active;
static uint32_t quake_audio_oneshot_since_ms;
static uint16_t quake_audio_loop_track;   /* 正在循环广播的曲目（软件记录，不依赖 DFPlayer 回读） */

static void quake_audio_enqueue(uint16_t track)
{
  if (quake_audio_count >= QUAKE_AUDIO_QUEUE_MAX)
  {
    return;
  }
  quake_audio_queue[quake_audio_tail] = track;
  quake_audio_tail = (uint8_t)((quake_audio_tail + 1U) % QUAKE_AUDIO_QUEUE_MAX);
  ++quake_audio_count;
}

static uint16_t quake_audio_dequeue(void)
{
  uint16_t track;

  if (quake_audio_count == 0U)
  {
    return 0U;
  }
  track = quake_audio_queue[quake_audio_head];
  quake_audio_head = (uint8_t)((quake_audio_head + 1U) % QUAKE_AUDIO_QUEUE_MAX);
  --quake_audio_count;
  return track;
}

/* 一次性事件语音是否已全部播完（队列空且无在播）。循环广播不计入。 */
static uint8_t quake_audio_done(void)
{
  return (quake_audio_oneshot_active == 0U && quake_audio_count == 0U) ? 1U : 0U;
}

/* 每个主循环调用：驱动一次性播报队列、检测播完、恢复/停止循环广播。 */
static void quake_audio_task(uint32_t now)
{
  QuakeState st = QuakeTask_GetState();
  QuakeTaskType typ = QuakeTask_GetTaskType();
  uint16_t desired_loop = 0U;

  /* 巡逻 / 确认中 + 对应任务才希望循环广播；否则停止。
     CONFIRMING 是「停车确认」的短暂中间态，广播先别停，否则每次停车确认
     都会把循环语音从头重播。 */
  if (st == QUAKE_STATE_PATROL || st == QUAKE_STATE_CONFIRMING)
  {
    if (typ == QUAKE_TASK_RESCUE)
    {
      desired_loop = QUAKE_AUDIO_TRACK_RESCUE_SEARCH;
    }
    else if (typ == QUAKE_TASK_DELIVER)
    {
      desired_loop = QUAKE_AUDIO_TRACK_DELIVER_TRANSPORT;
    }
  }

  /* 绕障 / 驱动故障期间不准循环广播，交给蜂鸣安全提示。 */
  if (LineObstacleBypass_GetState() != LINE_BYPASS_IDLE ||
      DriveBase_GetFaultMask() != 0U)
  {
    desired_loop = 0U;
  }

  /* 空闲且还有排队的一次性语音 => 播放下一首。 */
  if (quake_audio_oneshot_active == 0U && quake_audio_count != 0U)
  {
    uint16_t track = quake_audio_dequeue();
    (void)DfPlayerMini_SetLoopCurrent(0U);
    (void)DfPlayerMini_PlayMp3Track(track);
    quake_audio_oneshot_active = 1U;
    quake_audio_oneshot_since_ms = now;
  }

  /* 一次性语音播完（回到 STOPPED）=> 清标志，交由下方决定是否恢复循环。
     加了 120ms 去抖：IDLE->PLAYING 过渡期间 DFPlayer 可能短暂报 STOPPED，
     不能把「刚下发」误判成「已播完」。 */
  if (quake_audio_oneshot_active != 0U && DfPlayerMini_IsReady() &&
      (int32_t)(now - quake_audio_oneshot_since_ms) >= 120 &&
      DfPlayerMini_GetPlaybackState() == DFPLAYER_MINI_STOPPED)
  {
    quake_audio_oneshot_active = 0U;
  }

  /* 没有一次性语音在播/排队时，维持目标循环广播。
     用软件变量 quake_audio_loop_track 记录「正在循环的曲目」，不要回读
     DFPlayer 的 current track —— 后者（0x4C 查询）在 mp3 文件夹模式下可能
     返回物理序号、和文件夹序号不一致，会每帧重发 PLAY，结果只播开头几个音。 */
  if (quake_audio_oneshot_active == 0U && quake_audio_count == 0U)
  {
    if (desired_loop != 0U)
    {
      if (DfPlayerMini_IsReady() &&
          (DfPlayerMini_GetPlaybackState() == DFPLAYER_MINI_STOPPED ||
           quake_audio_loop_track != desired_loop))
      {
        quake_audio_loop_track = desired_loop;
        (void)DfPlayerMini_SetLoopCurrent(1U);
        (void)DfPlayerMini_PlayMp3Track(desired_loop);
      }
    }
    else
    {
      quake_audio_loop_track = 0U;
      /* 不需要广播：若 DFPlayer 仍在放（比如停了循环但没停干净），停掉。 */
      if (DfPlayerMini_IsReady() &&
          DfPlayerMini_GetPlaybackState() != DFPLAYER_MINI_STOPPED)
      {
        DfPlayerMini_Stop();
      }
    }
  }
}

/* ---------- 状态机事件 -> 入队一次性播报 ---------- */
static void quake_play_audio_event(QuakeAudioEvent event)
{
  switch (event)
  {
    case QUAKE_AUDIO_START:
      quake_audio_enqueue(QUAKE_AUDIO_TRACK_START);
      break;
    case QUAKE_AUDIO_TASK_RESCUE:
      quake_audio_enqueue(QUAKE_AUDIO_TRACK_TASK_RESCUE);
      break;
    case QUAKE_AUDIO_TASK_DELIVER:
      quake_audio_enqueue(QUAKE_AUDIO_TRACK_TASK_DELIVER);
      break;
    case QUAKE_AUDIO_RESCUE:
      /* 发现被困人员：先团队通报，再对被困者安抚。 */
      quake_audio_enqueue(QUAKE_AUDIO_TRACK_RESCUE_FOUND);
      quake_audio_enqueue(QUAKE_AUDIO_TRACK_RESCUE_LOCATED);
      break;
    case QUAKE_AUDIO_DELIVER:
      /* 到物资点：先通报投放完成，再广播发放指引。 */
      quake_audio_enqueue(QUAKE_AUDIO_TRACK_DELIVER_DONE);
      quake_audio_enqueue(QUAKE_AUDIO_TRACK_DELIVER_DISTRIBUTE);
      break;
    case QUAKE_AUDIO_DANGER:
      quake_audio_enqueue(QUAKE_AUDIO_TRACK_DANGER);
      break;
    case QUAKE_AUDIO_RESET:
      quake_audio_enqueue(QUAKE_AUDIO_TRACK_RESET);
      break;
    default:
      break;
  }
}

/* ---------- 蜂鸣：ACTION 长响，ALERT/翻车用短促报警（不刺耳） ---------- */
static void quake_beep(uint32_t now, uint8_t tilted)
{
  /* 说明：用 safety_override=0 只驱动 PG12 蜂鸣，不去停 DFPlayer——这样
     ACTION/ALERT/翻车时，扬声器照样播「救援/危险」语音，蜂鸣只做叠加。 */
  if (QuakeTask_GetState() == QUAKE_STATE_ACTION)
  {
    /* 识别到人/物资点：蜂鸣只响 QUAKE_ACTION_BEEP_MS 提示一下，之后静音。 */
    if (tick_reached(now, action_beep_until_ms) != 0U)
    {
      app_buzzer_safety_write(GPIO_PIN_RESET, 0U);
    }
    else
    {
      app_buzzer_safety_write(GPIO_PIN_SET, 0U);
    }
    return;
  }

  if (tilted == 0U && QuakeTask_GetAlertType() == QUAKE_ALERT_WARNING)
  {
    /* 余震预警（黄）：约每 2 秒短哔一下，最轻柔 */
    app_buzzer_safety_write(
        (now % 2000U) < 120U ? GPIO_PIN_SET : GPIO_PIN_RESET, 0U);
  }
  else
  {
    /* 危险区（红）/ 翻车：约每 1 秒短哔一下 */
    app_buzzer_safety_write(
        (now % 1000U) < 120U ? GPIO_PIN_SET : GPIO_PIN_RESET, 0U);
  }
}

/* ---------- 板载 LED（PG13/PG15）在实验七 MX_GPIO_Init 里未配置，这里补上 ---------- */
static void quake_led_init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOG_CLK_ENABLE();
  gpio.Pin = led1_Pin | led2_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(led1_GPIO_Port, &gpio);
  HAL_GPIO_WritePin(led1_GPIO_Port, led1_Pin | led2_Pin, GPIO_PIN_RESET);
}

/* ---------- 板载 LED 指示当前任务：救人=LED1，送物资=LED2 ---------- */
static void quake_leds(void)
{
  switch (QuakeTask_GetTaskType())
  {
    case QUAKE_TASK_RESCUE:
      HAL_GPIO_WritePin(led1_GPIO_Port, led1_Pin, GPIO_PIN_SET);
      HAL_GPIO_WritePin(led2_GPIO_Port, led2_Pin, GPIO_PIN_RESET);
      break;

    case QUAKE_TASK_DELIVER:
      HAL_GPIO_WritePin(led1_GPIO_Port, led1_Pin, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(led2_GPIO_Port, led2_Pin, GPIO_PIN_SET);
      break;

    default:
      HAL_GPIO_WritePin(led1_GPIO_Port, led1_Pin, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(led2_GPIO_Port, led2_Pin, GPIO_PIN_RESET);
      break;
  }
}

/* ---------- RGB 报警灯（声光报警的「光」）----------
 * 两个 RGB 灯：右灯 R/G/B 都在 PE2/PE3/PE4；左灯 R=PG1、G=PE7、B=PG2（跨端口）。
 * 高电平点亮（与参考工程 test-exp7-unified-motion-v1 一致）。
 * 巡逻 = 白光；危险区 = 红光闪烁、预警 = 黄光闪烁、发现人员 = 蓝光闪烁、
 * 物资点 = 绿光闪烁；翻车 = 常亮红；其余（待命/停车/等待派发）= 灭。 */
static void quake_rgb_set(uint8_t red, uint8_t green, uint8_t blue)
{
  GPIO_PinState r = red   ? GPIO_PIN_SET : GPIO_PIN_RESET;
  GPIO_PinState g = green ? GPIO_PIN_SET : GPIO_PIN_RESET;
  GPIO_PinState b = blue  ? GPIO_PIN_SET : GPIO_PIN_RESET;

  /* 右 RGB：RRGB_R=PE2 / RRGB_G=PE3 / RRGB_B=PE4 */
  HAL_GPIO_WritePin(RRGB_R_GPIO_Port, RRGB_R_Pin, r);
  HAL_GPIO_WritePin(RRGB_G_GPIO_Port, RRGB_G_Pin, g);
  HAL_GPIO_WritePin(RRGB_B_GPIO_Port, RRGB_B_Pin, b);

  /* 左 RGB：LRGB_R=PG1 / LRGB_G=PE7 / LRGB_B=PG2 */
  HAL_GPIO_WritePin(LRGB_R_GPIO_Port, LRGB_R_Pin, r);
  HAL_GPIO_WritePin(LRGB_G_GPIO_Port, LRGB_G_Pin, g);
  HAL_GPIO_WritePin(LRGB_B_GPIO_Port, LRGB_B_Pin, b);
}

static void quake_rgb_alarm(uint8_t tilted, uint32_t now)
{
  QuakeAlertType at = QuakeTask_GetAlertType();
  uint8_t on;

  if (tilted)
  {
    quake_rgb_set(1U, 0U, 0U);   /* 翻车 = 常亮红 */
    return;
  }

  /* 半周期 QUAKE_RGB_FLASH_MS，亮/灭交替 => 约 2 Hz 闪烁 */
  on = ((now / QUAKE_RGB_FLASH_MS) & 1U) ? 1U : 0U;

  switch (at)
  {
    case QUAKE_ALERT_DANGER:
      quake_rgb_set(on, 0U, 0U);   /* 危险区 = 红光闪烁 */
      break;
    case QUAKE_ALERT_WARNING:
      quake_rgb_set(on, on, 0U);   /* 余震预警 = 黄光闪烁 */
      break;
    case QUAKE_ALERT_PERSON:
      quake_rgb_set(0U, 0U, on);   /* 发现被困人员 = 蓝光闪烁 */
      break;
    case QUAKE_ALERT_SUPPLY:
      quake_rgb_set(0U, on, 0U);   /* 物资点 = 绿光闪烁 */
      break;
    default:
      /* 巡逻 / 确认中 = 白光；待命/停车/等待派发 = 灭 */
      if (QuakeTask_GetState() == QUAKE_STATE_PATROL ||
          QuakeTask_GetState() == QUAKE_STATE_CONFIRMING)
      {
        quake_rgb_set(1U, 1U, 1U);
      }
      else
      {
        quake_rgb_set(0U, 0U, 0U);
      }
      break;
  }
}

int main(void)
{
  uint32_t now;

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  quake_led_init();
  DiagnosticUart_Init();

  /* 驱动 + 循迹 + 避障（全部来自 motion/ 原样拷贝的代码）。 */
  motor_pwm_init();
  WheelEncoder_Init();
  WheelSpeedObserver_Init();
  WheelSpeedControl_Init();

  /* V2 黑线绕障控制器：红外 + 超声波一起触发，陀螺仪辅助转向。 */
  LineObstacleBypass_GetDefaultConfig(&bypass_config);
  bypass_config.emergency_speed_cps = QUAKE_EMERGENCY_BRAKE_SPEED_CPS;
  bypass_config.reverse_cps = QUAKE_BYPASS_REVERSE_CPS;
  bypass_config.forward_cps = QUAKE_BYPASS_FORWARD_CPS;
  bypass_config.clear_probe_cps = QUAKE_BYPASS_CLEAR_PROBE_CPS;
  bypass_config.return_cps = QUAKE_BYPASS_RETURN_CPS;
  bypass_config.turn_cps = QUAKE_BYPASS_TURN_CPS;
  LineObstacleBypass_Init(&bypass_config);

  ir_avoid_init();
  BatteryMonitor_Init();
  DriveBase_Init();
  line_tracking_init();
  Ultrasonic_Init();
  configure_ultrasonic_avoid();

  /* 扬声器（DFPlayer UART4） + 蜂鸣短语 + 断点续播。 */
  BuzzerPhrase400_Init();
  DfPlayerMini_Init();
  /* 循环/单曲由 quake_audio_task() 按需管理（loop-current 也由它切换）。 */
  quake_audio_head = quake_audio_tail = quake_audio_count = 0U;
  quake_audio_oneshot_active = 0U;

  /* 地震任务 + K210 视觉。 */
  vision_task_init();
  QuakeTask_Init();

  advanced_stop();
  ultrasonic_forward_speed_limit = 0;
  last_fast_speed_cps = 0U;
  last_fast_speed_ms = HAL_GetTick();
  next_bypass_direction = 1;
  bypass_rearm_pending = 0U;
  bypass_ir_clear_samples = 0U;
  bypass_rearm_not_before_ms = 0U;
  bypass_ir_trigger_candidate = 0U;
  bypass_ir_trigger_since_ms = HAL_GetTick();
  bypass_front_trigger_ms = 0U;
  bypass_front_sample_ms = 0U;
  bypass_front_obstacle = 0U;
  line_wait_side = 0;
  LineWaitGuard_Reset(&line_wait_guard);
  prev_quake_state = QUAKE_STATE_IDLE;
  tilt_active = 0U;
  action_beep_until_ms = 0U;
  avoidance_enabled = 0U;
  avoidance_armed = 0U;
  avoidance_arm_at_ms = 0U;

  /* 上电默认不无黑线直行，进入巡逻（模式一）时再打开。 */
  line_tracking_set_no_line_forward(0U);
  line_tracking_reset();

  DiagnosticUart_WriteString("QUAKE RESCUE V2 READY: KEY1=START KEY2=STOP\r\n");

  /* 红外发射管和 ADC 先预热；标定时车头前方保持无遮挡。 */
  HAL_Delay(1000U);
  if (!ir_avoid_calibrate())
  {
    ir_avoid_set_enabled(false);
  }

  /* 陀螺仪姿态传感器：上电后保持静置 2 秒标定零偏。 */
  MpuYaw_Init(HAL_GetTick());
  DiagnosticUart_WriteString("IMU: KEEP STILL 2s\r\n");

  while (1)
  {
    now = HAL_GetTick();
    QuakeVisionFrame frame;
    UltrasonicAvoidState us_state;
    IrAvoidReading ir_status;
    LineTrackingReading line;
    LineTrackingAction line_action;
    LineObstacleBypassInput bypass_input;
    LineObstacleBypassState bypass_state;
    uint32_t approach_speed_cps = 0U;
    uint16_t emergency_distance_cm = QUAKE_ULTRASONIC_STOP_CM;
    uint8_t tilted;
    uint8_t stationary;

    /* 1. 按键（低电平按下）：KEY1 启动，KEY2 停车。 */
    if (HAL_GPIO_ReadPin(key1_GPIO_Port, key1_Pin) == GPIO_PIN_RESET)
    {
      QuakeTask_Start();
    }
    if (HAL_GPIO_ReadPin(key2_GPIO_Port, key2_Pin) == GPIO_PIN_RESET)
    {
      QuakeTask_Stop();
    }

    /* 2. 扬声器 + 蜂鸣短语 + 陀螺仪（每个主循环都要服务）。 */
    DfPlayerMini_Task(now);
    BuzzerPhrase400_Task(now);

    {
      DriveBaseTelemetry drive;
      unsigned wheel;

      DriveBase_GetTelemetry(&drive);
      stationary = QuakeTask_GetState() == QUAKE_STATE_STOPPED &&
                   drive.mode == DRIVE_BASE_STOPPED;
      for (wheel = 0U; wheel < DRIVE_BASE_WHEEL_COUNT; ++wheel)
      {
        if (drive.requested_cps[wheel] || drive.measured_cps[wheel] > 30 ||
            drive.measured_cps[wheel] < -30 || drive.output_pwm[wheel])
        {
          stationary = 0U;
        }
      }
      if (stationary)
      {
        (void)GyroTurn_ClearTransientFault();
      }
      MpuYaw_Task(now, stationary);
    }

    /* 3. 驱动闭环 + 视觉帧 + 任务状态机（只做决策，不直接动电机）。 */
    DriveBase_Task(now);
    vision_task_poll();
    if (vision_task_take_frame(&frame))
    {
      QuakeTask_OnFrame(&frame, now);
    }

    /* 4. 状态机音频事件 -> 扬声器播报；语音播完后再推进状态机，
       保证「识别到人/危险/物资点后停下，播完语音再出发」。 */
    quake_play_audio_event(QuakeTask_TakeAudioEvent());
    quake_audio_task(now);
    QuakeTask_Task(now, quake_audio_done());

    /* 4b. 避障已整体关闭：红色危险仍会停车报警（见 ALERT 状态），但不再开启
       绕障/避障，之后一直保持纯循迹。这里照常取走红危险标志（避免残留）。 */
    (void)QuakeTask_TakeRedDanger();
    avoidance_armed = 0U;
    avoidance_enabled = 0U;

    /* 5. 巡逻 <-> 锁定态切换时，复位循迹/避障/绕障/陀螺的安全层。 */
    {
      QuakeState cur_state = QuakeTask_GetState();

      if (cur_state == QUAKE_STATE_PATROL &&
          prev_quake_state != QUAKE_STATE_PATROL)
      {
        line_tracking_start_following();
        configure_ultrasonic_avoid();
        WheelSpeedObserver_Start();
        last_fast_speed_cps = 0U;
        last_fast_speed_ms = now;
        next_bypass_direction = 1;
        bypass_rearm_pending = 0U;
        bypass_ir_clear_samples = 0U;
        bypass_rearm_not_before_ms = 0U;
        bypass_ir_trigger_candidate = 0U;
        bypass_ir_trigger_since_ms = now;
        LineWaitGuard_Reset(&line_wait_guard);
        /* 新一次出动/新任务默认关闭避障（纯循迹）；红危险触发后才开启。 */
        if (prev_quake_state == QUAKE_STATE_IDLE ||
            prev_quake_state == QUAKE_STATE_STOPPED ||
            prev_quake_state == QUAKE_STATE_WAIT_TASK)
        {
          avoidance_enabled = 0U;
          avoidance_armed = 0U;
        }
        DiagnosticUart_WriteString("QUAKE PATROL\r\n");
      }
      else if (cur_state != QUAKE_STATE_PATROL &&
               prev_quake_state == QUAKE_STATE_PATROL)
      {
        WheelSpeedObserver_Stop();
        LineObstacleBypass_Stop();
        GyroTurn_Stop();
        LineWaitGuard_Reset(&line_wait_guard);
        bypass_rearm_pending = 0U;
        bypass_ir_clear_samples = 0U;
        bypass_rearm_not_before_ms = 0U;
        bypass_ir_trigger_candidate = 0U;
        bypass_ir_trigger_since_ms = now;
        last_fast_speed_cps = 0U;
        last_fast_speed_ms = now;
        advanced_stop();
        line_tracking_reset();
        app_buzzer_safety_write(GPIO_PIN_RESET, 0U);
      }

      /* 全局停车：清空待播队列并停掉扬声器。 */
      if (cur_state == QUAKE_STATE_STOPPED)
      {
        quake_audio_head = quake_audio_tail = quake_audio_count = 0U;
        quake_audio_oneshot_active = 0U;
        quake_audio_loop_track = 0U;
        if (DfPlayerMini_IsReady())
        {
          DfPlayerMini_Stop();
        }
      }
      /* 进入「识别到目标」动作时，蜂鸣只响 QUAKE_ACTION_BEEP_MS，之后交给语音。 */
      if (cur_state == QUAKE_STATE_ACTION &&
          prev_quake_state != QUAKE_STATE_ACTION)
      {
        action_beep_until_ms = now + QUAKE_ACTION_BEEP_MS;
      }
      prev_quake_state = cur_state;
    }

    /* 6. 翻车/大倾角检测（陀螺仪加速度计）：危险就停车 + 警报。 */
    tilted = quake_is_tilted();
    if (tilted && tilt_active == 0U)
    {
      quake_audio_enqueue(QUAKE_AUDIO_TRACK_DANGER);
    }
    tilt_active = tilted;

    /* 6b. 声光报警的「光」：巡逻白光；红/黄/蓝/绿按事件闪烁；翻车常亮红。
        每帧刷新一次，离开对应状态后自动切回白光/熄灭。 */
    quake_rgb_alarm(tilted, now);

    /* 故障日志（STOPPED 态下通过调试串口可 dump）。 */
    LineFaultLog_Task((uint8_t)(QuakeTask_GetState() == QUAKE_STATE_STOPPED));

    /* 7. 停车 / 动作 / 避险 / 翻车：锁定电机，不循迹。 */
    if (QuakeTask_IsMotionLocked() || tilted)
    {
      advanced_stop();
      advanced_set_forward_speed_limit(MOTOR_PWM_PERIOD);
      if (tilted || QuakeTask_IsAlerting())
      {
        quake_beep(now, tilted);
      }
      else
      {
        app_buzzer_safety_write(GPIO_PIN_RESET, 0U);
      }
      quake_leds();
      HAL_Delay(1U);
      continue;
    }

    /* 8. 巡逻：卡死看门（丢线自动搜索）。 */
    if (service_quake_line_wait())
    {
      quake_leds();
      HAL_Delay(1U);
      continue;
    }

    /* 9. 驱动故障 -> 蜂鸣 + 亮灯，等人工处理。 */
    if (DriveBase_GetFaultMask() != 0U &&
        LineObstacleBypass_GetState() == LINE_BYPASS_IDLE)
    {
      uint8_t fault_code = encoder_fault_beep_code(DriveBase_GetFaultMask());
      uint32_t phase = now % 2500U;

      app_buzzer_safety_write(
          (phase < (uint32_t)fault_code * 250U &&
           (phase % 250U) < 100U) ? GPIO_PIN_SET : GPIO_PIN_RESET, 1U);
      HAL_GPIO_WritePin(led1_GPIO_Port, led1_Pin, GPIO_PIN_SET);
      HAL_GPIO_WritePin(led2_GPIO_Port, led2_Pin, GPIO_PIN_SET);
      HAL_Delay(1U);
      continue;
    }

    /* 10. 巡逻：避障关闭时纯循迹；避障开启后红外 + 超声波 -> V2 绕障 + 循迹。 */
    quake_leds();

    if (avoidance_enabled == 0U)
    {
      /* 避障关闭（默认）：纯四路黑线闭环循迹，不查红外/超声波/绕障。 */
      advanced_set_forward_speed_limit(MOTOR_PWM_PERIOD);
      (void)line_tracking_follow_once(QUAKE_LINE_SPEED, MOTOR_PWM_PERIOD);
      HAL_Delay(1U);
      continue;
    }

    ir_status = ir_avoid_read();
    ir_avoid_show_status(&ir_status);

    if (LineObstacleBypass_GetState() == LINE_BYPASS_IDLE)
    {
      WheelSpeedObserver_Task();
      approach_speed_cps = app_approach_speed_cps();
      emergency_distance_cm = app_emergency_distance_cm(approach_speed_cps);
    }

    /* 绕障完成后：等红外离开障碍再重新布防。 */
    if (bypass_rearm_pending != 0U)
    {
      if (!ir_status.left_obstacle && !ir_status.right_obstacle)
      {
        if (bypass_ir_clear_samples < QUAKE_BYPASS_REARM_CLEAR_SAMPLES)
        {
          ++bypass_ir_clear_samples;
        }
      }
      else
      {
        bypass_ir_clear_samples = 0U;
      }
      if (tick_reached(now, bypass_rearm_not_before_ms) &&
          bypass_ir_clear_samples >= QUAKE_BYPASS_REARM_CLEAR_SAMPLES)
      {
        bypass_rearm_pending = 0U;
      }
      else
      {
        ir_status.left_obstacle = false;
        ir_status.right_obstacle = false;
      }
    }

    /* 绕障中：独占电机；喂 1ms 循迹采样队列 + 前方超声波给绕障控制器。 */
    if (LineObstacleBypass_GetState() != LINE_BYPASS_IDLE)
    {
      LineSensorSample sample;
      uint32_t through_ms = now;

      while (LineSensorSample_PopThrough(&sample, through_ms))
      {
        LineObstacleBypass_ObserveRawSensors(sample.mask, sample.time_ms);
      }

      {
        uint16_t cm;
        uint8_t result;

        Ultrasonic_Task();
        result = Ultrasonic_GetResult(&cm);
        if (result == ULTRASONIC_RESULT_OK)
        {
          bypass_front_sample_ms = now;
          bypass_front_obstacle = cm <= QUAKE_ULTRASONIC_STOP_CM;
        }
        if (now - bypass_front_sample_ms > 250U)
        {
          bypass_front_obstacle = 0U;
        }
        if (!Ultrasonic_IsBusy() &&
            now - bypass_front_trigger_ms >= 60U && Ultrasonic_Start())
        {
          bypass_front_trigger_ms = now;
        }
      }

      line = line_tracking_read();
      make_bypass_input(&bypass_input, &line, &ir_status);
      bypass_input.front_obstacle = bypass_front_obstacle;
      LineObstacleBypass_Task(&bypass_input);
      bypass_state = LineObstacleBypass_GetState();

      if (bypass_state == LINE_BYPASS_FAULT)
      {
        uint8_t fault_code = encoder_fault_beep_code(
            LineObstacleBypass_GetFaultMask());
        uint32_t phase = now % 2500U;

        app_buzzer_safety_write(
            (phase < (uint32_t)fault_code * 250U &&
             (phase % 250U) < 100U) ? GPIO_PIN_SET : GPIO_PIN_RESET, 1U);
      }
      else
      {
        app_buzzer_safety_write(GPIO_PIN_RESET, 1U);
      }

      if (bypass_state == LINE_BYPASS_DONE)
      {
        uint8_t contact = LineObstacleBypass_GetCapturedLineMask();

        LineObstacleBypass_Stop();
        line_tracking_rejoin_from_bypass(contact);
        bypass_rearm_pending = 1U;
        bypass_ir_clear_samples = 0U;
        bypass_rearm_not_before_ms = now + QUAKE_BYPASS_REARM_DELAY_MS;
        UltrasonicAvoid_ResumeFollowing();
        (void)line_tracking_follow_once(QUAKE_LINE_SPEED,
                                        ultrasonic_forward_speed_limit);
        WheelSpeedObserver_Start();
        last_fast_speed_cps = 0U;
        last_fast_speed_ms = now;
        /* 绕障结束、已重新回到黑线：关闭避障模式，恢复纯循迹。 */
        avoidance_enabled = 0U;
        avoidance_armed = 0U;
        DiagnosticUart_WriteString("BYP DONE\r\n");
      }
      else if (bypass_state == LINE_BYPASS_FAULT)
      {
        HAL_GPIO_WritePin(LRGB_R_GPIO_Port, LRGB_R_Pin, GPIO_PIN_SET);
      }
      HAL_Delay(1U);
      continue;
    }

    /* 红外方向确认 -> 启动绕障。 */
    {
      int8_t confirmed_direction;

      if (confirmed_ir_bypass_direction(&ir_status,
                                        &confirmed_direction) != 0U)
      {
        line_tracking_reset();
        WheelSpeedObserver_Stop();
        (void)LineObstacleBypass_StartWithSpeed(confirmed_direction,
                                                approach_speed_cps);
        DiagnosticUart_WriteString("BYP IR\r\n");
        HAL_Delay(1U);
        continue;
      }
    }

    /* 超声波避障状态机：动作阶段独占电机，命中立即交给绕障脱困。 */
    advanced_set_forward_speed_limit(MOTOR_PWM_PERIOD);
    UltrasonicAvoid_SetEmergencyDistance(emergency_distance_cm);
    UltrasonicAvoid_Task();
    us_state = UltrasonicAvoid_GetState();

    if (us_state == ULTRASONIC_AVOID_STOPPING ||
        us_state == ULTRASONIC_AVOID_BACKING ||
        us_state == ULTRASONIC_AVOID_GUARD ||
        us_state == ULTRASONIC_AVOID_TURNING)
    {
      line_tracking_reset();
      WheelSpeedObserver_Stop();
      (void)LineObstacleBypass_StartWithSpeed(
          choose_bypass_direction(&ir_status), approach_speed_cps);
      DiagnosticUart_WriteString("BYP US\r\n");
      HAL_Delay(1U);
      continue;
    }

    if (us_state != ULTRASONIC_AVOID_FORWARD)
    {
      advanced_set_forward_speed_limit(MOTOR_PWM_PERIOD);
      HAL_Delay(1U);
      continue;
    }
    if (ultrasonic_forward_speed_limit <= 0)
    {
      advanced_stop();
      HAL_Delay(1U);
      continue;
    }

    advanced_set_forward_speed_limit(ultrasonic_forward_speed_limit);

    /* 四路黑线闭环循迹（限速由超声波安全层提供）。 */
    line_action = line_tracking_follow_once(QUAKE_LINE_SPEED,
                                            ultrasonic_forward_speed_limit);
    if (line_action != LINE_ACTION_FORWARD &&
        line_action != LINE_ACTION_CROSSING)
    {
      UltrasonicMotion_Reset();
    }

    HAL_Delay(1U);
  }
}

/* PF12/ECHO 使用 EXTI15_10；地震项目不用红外遥控，只转超声波。 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  Ultrasonic_EXTI_Callback(GPIO_Pin);
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
    HAL_GPIO_TogglePin(led1_GPIO_Port, led1_Pin);
    HAL_Delay(150U);
  }
}
