/*
 * 单片机实验七：循迹、避障、视觉综合
 * 目标芯片：STM32F103ZETx（YB-DSF01-V1.1）
 *
 * 运行逻辑：
 *   - 上电默认锁存停车；按下 KEY1 后进入综合模式。前方红外/超声波
 *     触发 V2 黑线绕障控制器；直线段由编码器限制距离，转弯段由
 *     MPU6050 实测相对偏航角，随后在障碍另一侧重新捕获黑线。
 *   - 按下 KEY2：纯寻线模式，红外、超声波和视觉不再控制电机。
 *   - 模式 3：KEY2 慢速循迹 + K210 传感器优先圆环导航。
 *   - 模式 4：K210 选边，MPU6050 控制切线入弧、半圆和出口航向。
 *   - 遥控数字 5：固定绕障 + 快速四线循迹，不依赖 K210。
 *   - 数字 0 随时停车；各模式都会在松开按键后保持。
 *
 * 第一次上电请让车轮离地。电脑端“编译通过”不等于车辆已经完成地面
 * 实测，参数仍需在实际黑线宽度、电池电量和负载下微调。
 */

#include "main.h"
#include "line_fault_log.h"
#include "sign_trace.h"
#include "sign_horn.h"
#include "gpio.h"

#include <stdint.h>

#include "audio_resume_store.h"
#include "battery_monitor.h"
#include "buzzer_phrase_40077493715.h"
#include "dfplayer_mini.h"
#include "diagnostic_uart.h"
#include "drive_base.h"
#include "encoder_linear.h"
#include "encoder_straight.h"
#include "encoder_turn.h"
#include "figure8_encoder.h"
#include "ir_avoid.h"
#include "ir_remote.h"
#include "line_obstacle_bypass.h"
#include "line_bypass_range.h"
#include "line_tracking.h"
#include "line_recovery.h"
#include "line_wait_guard.h"
#include "mpu6050_yaw.h"
#include "gyro_turn.h"
#include "line_bypass_turn.h"
#include "sign_route.h"
#include "sign_observation.h"
#include "line_sensor_sample.h"
#include "sign_line_follow.h"
#if defined(LINE_TRACKING_LIFT_TEST)
#include "line_tracking_lift_test.h"
#endif
#include "motion_advanced.h"
#include "motorPWM.h"
#include "oled_status.h"
#include "square_encoder.h"
#include "ultrasonic.h"
#include "ultrasonic_avoid.h"
#include "ultrasonic_motion.h"
#include "vision_uart.h"
#include "wheel_encoder.h"
#include "wheel_speed_observer.h"
#include "wheel_speed_control.h"

/* 综合模式参数。PWM 周期为 3599。 */
#define EXP7_LINE_SPEED                 3000
/* Temporary fixed-route isolation test; remote IR and line probes are separate. */
#define EXP7_IR_AVOID_ENABLED              0U
#define EXP7_LIMIT_SPEED               2200

/* 超声波安全层参数。距离阈值由 ultrasonic_avoid.c 实现，速度在这里
   与整体工程的循迹/视觉速度对齐。 */
/* Clear-path limit leaves PWM headroom for differential steering; the line
   controller itself limits straight travel to EXP7_LINE_SPEED. */
#define EXP7_ULTRASONIC_CRUISE_SPEED   3599
#define EXP7_ULTRASONIC_SLOW_SPEED     2600
/* Earlier approach trigger; keep the close guard within an active detour
   separate from the approach threshold to avoid clipping every corner. */
#define EXP7_ULTRASONIC_STOP_CM          16U
#define EXP7_ULTRASONIC_CLEAR_CM         28U
#define EXP7_ULTRASONIC_EMERGENCY_MAX_CM 22U
#define EXP7_ULTRASONIC_LOOKAHEAD_MS    100U
#define EXP7_ULTRASONIC_BYPASS_STOP_CM   15U
#define EXP7_ASSUMED_FAST_SPEED_CPS    5300U
#define EXP7_EMERGENCY_BRAKE_SPEED_CPS 3500U
#define EXP7_FAST_SPEED_HOLD_MS          220U
#define EXP7_ENCODER_COUNTS_PER_REV    1040U
#define EXP7_WHEEL_DIAMETER_MM           47U
#define EXP7_PI_X10000                 31416U
#define EXP7_ULTRASONIC_TURN_INNER     2800
#define EXP7_ULTRASONIC_TURN_OUTER     3300
#define EXP7_ULTRASONIC_TURN_TIME_MS    500U
#define EXP7_ULTRASONIC_REVERSE_SPEED  2600
#define EXP7_ULTRASONIC_STOP_TIME_MS    120U
#define EXP7_ULTRASONIC_REVERSE_TIME_MS 300U
#define EXP7_ULTRASONIC_GUARD_TIME_MS    60U
#define EXP7_ULTRASONIC_NO_ECHO_COUNT     3U
#define EXP7_BYPASS_REVERSE_CPS        1900U
#define EXP7_BYPASS_FORWARD_CPS        4000U
#define EXP7_BYPASS_CLEAR_PROBE_CPS    2200U
#define EXP7_BYPASS_RETURN_CPS         4000U
#define EXP7_BYPASS_TURN_CPS           2500U
#define EXP7_PASSIVE_MEASURE_INTERVAL_MS  70U
#define EXP7_VISION_ENABLED                 0U

/* 视觉事件动作时序。 */
#define EXP7_VISION_TURN_INNER_SPEED     900
#define EXP7_VISION_TURN_OUTER_SPEED    2500
#define EXP7_VISION_TURN_TIME_MS         360U
#define EXP7_GARAGE_SPEED              1500
#define EXP7_GARAGE_TIME_MS            2000U
#define EXP7_HORN_TIMEOUT_MS           1700U
#define EXP7_STOP_HOLD_MS              1000U
#define EXP7_AUDIO_VOLUME_STEP             2

typedef enum
{
  VISION_ACTION_NONE = 0,
  VISION_ACTION_TURN_LEFT,
  VISION_ACTION_TURN_RIGHT,
  VISION_ACTION_GARAGE_ONE,
  VISION_ACTION_GARAGE_TWO,
  VISION_ACTION_HORN,
  VISION_ACTION_STOP
} VisionAction;

typedef enum
{
  APP_MODE_INTEGRATED = 0U,
  APP_MODE_LINE_ONLY,
  APP_MODE_SIGN_LINE,
  APP_MODE_SIGN_GYRO_TANGENT,
  APP_MODE_FIXED_BYPASS,
  APP_MODE_STOPPED
} AppMode;

static VisionAction vision_action;
static uint32_t vision_action_deadline;
static int16_t line_speed;
static int16_t ultrasonic_forward_speed_limit;
static uint32_t last_motion_telemetry_ms;
static uint32_t passive_measure_trigger_ms;
static uint32_t last_oled_update_ms;
static uint32_t last_sign_uart_ms;
static uint32_t last_bypass_uart_ms;
static uint32_t last_battery_uart_ms;
static uint32_t last_drive_base_uart_ms;
static uint32_t last_fast_speed_cps;
static uint32_t last_fast_speed_ms;
static int8_t next_bypass_direction;
static uint8_t bypass_rearm_pending;
static uint8_t bypass_ir_clear_samples;
static uint32_t bypass_rearm_not_before_ms;
static uint8_t bypass_ir_trigger_candidate;
static uint32_t bypass_ir_trigger_since_ms;
static LineWaitGuard line_wait_guard;
static uint8_t imu_dump_requested, imu_calibrate_requested;
static uint32_t imu_generation;
static int8_t line_wait_side;
static SignLineFollowController sign_line_controller;
static uint8_t sign_line_mask;
static uint8_t sign_line_action;
static uint8_t fixed_bypass_mode;
static uint8_t audio_store_error_reported;

#define BYPASS_REARM_DELAY_MS          1000U
#define BYPASS_REARM_CLEAR_SAMPLES       10U
#define BYPASS_IR_TRIGGER_CONFIRM_MS      30U

void SystemClock_Config(void);

static void app_drive_forward(int16_t left_speed, int16_t right_speed);
static void app_drive_backward(int16_t left_speed, int16_t right_speed);
static void app_stop(void);
static void app_turn_left(int16_t inner_speed, int16_t outer_speed);
static void app_turn_right(int16_t inner_speed, int16_t outer_speed);
static void configure_ultrasonic_avoid(void);
static AppMode read_requested_mode(AppMode current_mode);
static uint8_t app_take_serial_virtual_key(void);
static void app_audio_toggle(void);
static void app_audio_next(void);
static void app_audio_previous(void);
static void app_audio_adjust_volume(int8_t delta);
static void app_audio_persist_task(uint8_t allow_page_erase);
static uint8_t tick_reached(uint32_t now, uint32_t deadline);
static uint8_t encoder_fault_beep_code(uint8_t fault_mask);
static uint32_t app_approach_speed_cps(void);
static uint16_t app_emergency_distance_cm(uint32_t speed_cps);
static void app_buzzer_safety_write(GPIO_PinState output,
                                    uint8_t safety_override);
static void cancel_vision_action(void);
static void start_vision_command(VisionCommand command, uint32_t now);
static uint8_t run_vision_action(uint32_t now);
static void experiment7_integrated_once(void);
static void show_ultrasonic_fault(UltrasonicAvoidState state);
static void update_ultrasonic_buzzer(UltrasonicAvoidState state);
static void motion_telemetry_task(void);
static void passive_ultrasonic_motion_task(void);
static void oled_application_task(AppMode mode);
static void battery_telemetry_task(void);
static void drive_base_telemetry_task(void);
static void vision_line_v4_diagnostic_dump(void);
static void make_bypass_input(LineObstacleBypassInput *input,
                              const LineTrackingReading *line,
                              const IrAvoidReading *infrared);
static void bypass_telemetry_task(void);
static uint8_t line_reading_mask(const LineTrackingReading *line);
static void sign_line_task(AppMode mode);

static uint8_t tick_reached(uint32_t now, uint32_t deadline)
{
  return (int32_t)(now - deadline) >= 0 ? 1U : 0U;
}

static uint32_t app_approach_speed_cps(void)
{
  uint32_t speed_cps = 0U;
  uint32_t now = HAL_GetTick();

  if (WheelSpeedObserver_GetAverageCps(&speed_cps) != 0U)
  {
    if (speed_cps >= EXP7_EMERGENCY_BRAKE_SPEED_CPS)
    {
      last_fast_speed_cps = speed_cps;
      last_fast_speed_ms = now;
    }
    else if (last_fast_speed_cps != 0U &&
             now - last_fast_speed_ms <= EXP7_FAST_SPEED_HOLD_MS)
    {
      /* Preserve the pre-slowdown speed across the two ultrasonic confirm
         samples so emergency braking is selected from the approach speed,
         not from the already reduced speed. */
      speed_cps = last_fast_speed_cps;
    }
    else
    {
      last_fast_speed_cps = 0U;
    }
    return speed_cps;
  }

  /* During the first observer window, assume the measured full-speed profile
     rather than shrinking the safety distance on missing speed data. */
  last_fast_speed_cps = EXP7_ASSUMED_FAST_SPEED_CPS;
  last_fast_speed_ms = now;
  return EXP7_ASSUMED_FAST_SPEED_CPS;
}

static uint16_t app_emergency_distance_cm(uint32_t speed_cps)
{
  uint64_t speed_mm_s;
  uint64_t lookahead_cm;
  uint32_t distance_cm;

  speed_mm_s = ((uint64_t)speed_cps * EXP7_PI_X10000 *
                EXP7_WHEEL_DIAMETER_MM +
                (uint64_t)EXP7_ENCODER_COUNTS_PER_REV * 5000ULL) /
               ((uint64_t)EXP7_ENCODER_COUNTS_PER_REV * 10000ULL);
  lookahead_cm = (speed_mm_s * EXP7_ULTRASONIC_LOOKAHEAD_MS + 9999ULL) /
                 10000ULL;
  distance_cm = EXP7_ULTRASONIC_STOP_CM + (uint32_t)lookahead_cm;
  if (distance_cm > EXP7_ULTRASONIC_EMERGENCY_MAX_CM)
  {
    distance_cm = EXP7_ULTRASONIC_EMERGENCY_MAX_CM;
  }
  if (distance_cm >= EXP7_ULTRASONIC_CLEAR_CM)
  {
    distance_cm = EXP7_ULTRASONIC_CLEAR_CM - 1U;
  }
  return (uint16_t)distance_cm;
}

static int8_t choose_bypass_direction(const IrAvoidReading *reading)
{
  int8_t direction;

  /* Obstacle on the left -> bypass right, and vice versa.  If both/neither
     IR channels identify a side (for example ultrasonic-only detection),
     alternate successive detours so the car does not always prefer one side. */
  if (reading->left_obstacle && !reading->right_obstacle)
  {
    return 1;
  }
  if (reading->right_obstacle && !reading->left_obstacle)
  {
    return -1;
  }
  direction = next_bypass_direction;
  next_bypass_direction = (int8_t)-next_bypass_direction;
  return direction;
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
                        BYPASS_IR_TRIGGER_CONFIRM_MS) == 0U)
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
  input->left_ir_adc = infrared->left_adc;
  input->right_ir_adc = infrared->right_adc;
  input->left_ir_threshold = ir_avoid_get_left_threshold();
  input->right_ir_threshold = ir_avoid_get_right_threshold();
  input->left_ir_hysteresis = ir_avoid_get_left_hysteresis();
  input->right_ir_hysteresis = ir_avoid_get_right_hysteresis();
}

static void bypass_telemetry_task(void)
{
  LineObstacleBypassTelemetry telemetry;
  uint32_t now = HAL_GetTick();

  if (LineObstacleBypass_GetState() == LINE_BYPASS_IDLE ||
      !tick_reached(now, last_bypass_uart_ms + 200U))
  {
    return;
  }

  last_bypass_uart_ms = now;
  LineObstacleBypass_GetTelemetry(&telemetry);
  DiagnosticUart_WriteString("BYP2 S=");
  DiagnosticUart_WriteUnsigned((uint32_t)telemetry.state);
  DiagnosticUart_WriteString(" D=");
  DiagnosticUart_WriteSigned((int32_t)telemetry.bypass_direction);
  DiagnosticUart_WriteString(" I=");
  DiagnosticUart_WriteUnsigned((uint32_t)telemetry.motion_intent);
  DiagnosticUart_WriteString(" IR=");
  DiagnosticUart_WriteUnsigned(telemetry.inside_ir_adc);
  DiagnosticUart_WriteString(" B=");
  DiagnosticUart_WriteUnsigned(telemetry.inside_ir_lower);
  DiagnosticUart_WriteString("..");
  DiagnosticUart_WriteUnsigned(telemetry.inside_ir_upper);
  DiagnosticUart_WriteString(" L=");
  DiagnosticUart_WriteUnsigned(telemetry.line_mask);
  DiagnosticUart_WriteString(" LC=");
  DiagnosticUart_WriteUnsigned(telemetry.original_line_cleared);
  DiagnosticUart_WriteString(" FL=");
  DiagnosticUart_WriteUnsigned(telemetry.flank_acquired);
  DiagnosticUart_WriteString(" AC=");
  DiagnosticUart_WriteUnsigned(telemetry.acquire_escape_committed);
  DiagnosticUart_WriteString(" AT=");
  DiagnosticUart_WriteUnsigned(telemetry.acquire_travel_mm);
  DiagnosticUart_WriteString(" FT=");
  DiagnosticUart_WriteUnsigned(telemetry.flank_travel_mm);
  DiagnosticUart_WriteString(" RA=");
  DiagnosticUart_WriteUnsigned(telemetry.return_aligned);
  DiagnosticUart_WriteString(" RT=");
  DiagnosticUart_WriteSigned(telemetry.return_target_mdeg / 1000L);
  DiagnosticUart_WriteString(" RYAW="); DiagnosticUart_WriteSigned(telemetry.return_yaw_mdeg);
  DiagnosticUart_WriteString(" RV="); DiagnosticUart_WriteUnsigned(telemetry.return_yaw_valid);
  DiagnosticUart_WriteString(" RC="); DiagnosticUart_WriteUnsigned(telemetry.return_cruise);
  DiagnosticUart_WriteString(" CAP="); DiagnosticUart_WriteUnsigned(telemetry.captured_line_mask);
  DiagnosticUart_WriteString(" FIX="); DiagnosticUart_WriteUnsigned(telemetry.fixed_route_phase);
  DiagnosticUart_WriteString(" FB="); DiagnosticUart_WriteUnsigned(telemetry.fixed_route_fallback);
  DiagnosticUart_WriteString(" IRON="); DiagnosticUart_WriteUnsigned(telemetry.infrared_enabled);
  DiagnosticUart_WriteString(" RM=");
  DiagnosticUart_WriteUnsigned(telemetry.return_travel_mm);
  DiagnosticUart_WriteString(" V=");
  DiagnosticUart_WriteUnsigned(telemetry.entry_speed_cps);
  DiagnosticUart_WriteString(" EB=");
  DiagnosticUart_WriteUnsigned(telemetry.emergency_brake_active);
  DiagnosticUart_WriteString(" CT=");
  DiagnosticUart_WriteUnsigned(telemetry.guided_turn_active);
  DiagnosticUart_WriteString(" C=");
  DiagnosticUart_WriteUnsigned(telemetry.clear_probe_steps);
  DiagnosticUart_WriteString(" MM=");
  DiagnosticUart_WriteUnsigned(telemetry.segment_progress_mm);
  DiagnosticUart_WriteString(" A=");
  DiagnosticUart_WriteSigned(telemetry.net_turn_mdeg / 1000L);
  DiagnosticUart_WriteString(" F=");
  DiagnosticUart_WriteUnsigned(telemetry.fault_mask);
  DiagnosticUart_WriteString("\r\n");
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

  if ((fault_mask & 0x10U) != 0U) return 5U; /* controller timeout */
  if ((fault_mask & 0x20U) != 0U) return 6U; /* net turn reached 360 deg */
  if ((fault_mask & 0x40U) != 0U) return 7U; /* side IR invalid */
  if ((fault_mask & 0x80U) != 0U) return 8U; /* invalid input */
  return 1U;
}

/*
 * Audio safety arbitration:
 *   safety_override=1 is used by stop/fault/ultrasonic/bypass warnings and
 *   immediately cancels the lower-priority DFPlayer request and PG12 phrase
 *   before driving the buzzer pin;
 *   safety_override=0 merely releases an inactive warning and never truncates
 *   a phrase which is already playing.
 */
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
    HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET);
  }
}

static uint8_t app_take_serial_virtual_key(void)
{
  int16_t value = DiagnosticUart_ReadChar();

  switch (value)
  {
    case 'g':
    case 'G':
      imu_dump_requested = 1U;
      return IR_REMOTE_VIRTUAL_KEY_NONE;
    case 'c':
    case 'C':
      imu_calibrate_requested = 1U;
      return IR_REMOTE_VIRTUAL_KEY_NONE;
    case 'f':
    case 'F':
      LineFaultLog_RequestDump();
      return IR_REMOTE_VIRTUAL_KEY_NONE;
    case 'j':
      SignTrace_Request(0U);
      return IR_REMOTE_VIRTUAL_KEY_NONE;
    case 'J':
      SignTrace_Request(1U);
      return IR_REMOTE_VIRTUAL_KEY_NONE;
    case '0':
      DfPlayerMini_Stop();
      BuzzerPhrase400_Stop();
      return IR_REMOTE_VIRTUAL_STOP;
    case '1': return IR_REMOTE_VIRTUAL_KEY1;
    case '2': return IR_REMOTE_VIRTUAL_KEY2;
    case '3': return IR_REMOTE_VIRTUAL_KEY3;
    case '4': return IR_REMOTE_VIRTUAL_KEY4;
    case '5': return IR_REMOTE_VIRTUAL_KEY5;
    case 'v':
    case 'V':
      vision_line_v4_diagnostic_dump();
      return IR_REMOTE_VIRTUAL_KEY_NONE;
    case 'b':
      app_audio_toggle();
      return IR_REMOTE_VIRTUAL_KEY_NONE;
    case 'n':
    case 'N':
      app_audio_next();
      return IR_REMOTE_VIRTUAL_KEY_NONE;
    case 'p':
    case 'P':
      app_audio_previous();
      return IR_REMOTE_VIRTUAL_KEY_NONE;
    case '+':
      app_audio_adjust_volume(EXP7_AUDIO_VOLUME_STEP);
      return IR_REMOTE_VIRTUAL_KEY_NONE;
    case '-':
      app_audio_adjust_volume(-EXP7_AUDIO_VOLUME_STEP);
      return IR_REMOTE_VIRTUAL_KEY_NONE;
    case 'B':
      (void)BuzzerPhrase400_Start(6U);
      DiagnosticUart_WriteString("BUZZER PHRASE x6\r\n");
      return IR_REMOTE_VIRTUAL_KEY_NONE;
    case 'x':
    case 'X':
      DfPlayerMini_Stop();
      BuzzerPhrase400_Stop();
      DiagnosticUart_WriteString("AUDIO STOP\r\n");
      return IR_REMOTE_VIRTUAL_KEY_NONE;
    default:  return IR_REMOTE_VIRTUAL_KEY_NONE;
  }
}

static void app_audio_toggle(void)
{
  if (DfPlayerMini_TogglePlayPause() != 0U)
  {
    DfPlayerMiniPlaybackState state = DfPlayerMini_GetPlaybackState();
    DiagnosticUart_WriteString(state == DFPLAYER_MINI_PAUSED ?
                               "DFPLAYER PAUSE TRACK=" :
                               "DFPLAYER PLAY/RESUME TRACK=");
    DiagnosticUart_WriteUnsigned(DfPlayerMini_GetCurrentTrack());
    DiagnosticUart_WriteString(" LOOP=CURRENT\r\n");
  }
}

static void app_audio_next(void)
{
  if (DfPlayerMini_Next() != 0U)
  {
    DiagnosticUart_WriteString("DFPLAYER NEXT; LOOP=CURRENT\r\n");
  }
}

static void app_audio_previous(void)
{
  if (DfPlayerMini_Previous() != 0U)
  {
    DiagnosticUart_WriteString("DFPLAYER PREVIOUS; LOOP=CURRENT\r\n");
  }
}

static void app_audio_adjust_volume(int8_t delta)
{
  if (DfPlayerMini_AdjustVolume(delta) != 0U)
  {
    DiagnosticUart_WriteString("DFPLAYER VOLUME=");
    DiagnosticUart_WriteUnsigned(DfPlayerMini_GetVolume());
    DiagnosticUart_WriteString("/30\r\n");
  }
}

static void app_audio_persist_task(uint8_t allow_page_erase)
{
  uint16_t track_number;
  AudioResumeStoreResult result;

  if (DfPlayerMini_TakeTrackChanged(&track_number) != 0U)
  {
    (void)AudioResumeStore_RequestTrack(track_number);
  }

  result = AudioResumeStore_Task(allow_page_erase);
  if (result == AUDIO_RESUME_STORE_SAVED)
  {
    audio_store_error_reported = 0U;
    DiagnosticUart_WriteString("DFPLAYER REMEMBER TRACK=");
    DiagnosticUart_WriteUnsigned(AudioResumeStore_GetTrack());
    DiagnosticUart_WriteString("\r\n");
  }
  else if (result == AUDIO_RESUME_STORE_ERROR &&
           audio_store_error_reported == 0U)
  {
    audio_store_error_reported = 1U;
    DiagnosticUart_WriteString("DFPLAYER TRACK MEMORY ERROR\r\n");
  }
}

/* 超声波模块的前进回调只发布“安全速度上限”，不直接抢占整体工程的
   循迹/视觉控制；这样测距安全层与原有模式可以组合。转弯和停车则由
   超声波状态机直接接管，避免障碍确认期间继续执行普通动作。 */
static void app_drive_forward(int16_t left_speed, int16_t right_speed)
{
  int16_t limit = left_speed < right_speed ? left_speed : right_speed;

  if (limit < 0)
  {
    limit = 0;
  }
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
  /* 前方已经靠墙时不能继续向前画弧，改为原地旋转。 */
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
  UltrasonicAvoid_Init(app_drive_forward,
                       app_stop,
                       app_turn_left,
                       app_turn_right);
  UltrasonicAvoid_SetThresholds(EXP7_ULTRASONIC_STOP_CM,
                                 EXP7_ULTRASONIC_CLEAR_CM);
  UltrasonicAvoid_SetEmergencyDistance(EXP7_ULTRASONIC_STOP_CM);
  UltrasonicAvoid_SetSpeeds(EXP7_ULTRASONIC_CRUISE_SPEED,
                            EXP7_ULTRASONIC_SLOW_SPEED,
                            EXP7_ULTRASONIC_TURN_INNER,
                            EXP7_ULTRASONIC_TURN_OUTER);
  UltrasonicAvoid_SetTurnTime(EXP7_ULTRASONIC_TURN_TIME_MS);
  UltrasonicAvoid_SetEscapeManeuver(app_drive_backward,
                                    EXP7_ULTRASONIC_REVERSE_SPEED,
                                    EXP7_ULTRASONIC_STOP_TIME_MS,
                                    EXP7_ULTRASONIC_REVERSE_TIME_MS,
                                    EXP7_ULTRASONIC_GUARD_TIME_MS);
  /* 开阔区超过量程时允许低速降级；断线与超量程无法仅靠 ECHO
     完全区分，因此该策略只在本综合工程中显式启用。 */
  UltrasonicAvoid_SetNoEchoFallback(1U, EXP7_ULTRASONIC_NO_ECHO_COUNT);
}

static void show_ultrasonic_fault(UltrasonicAvoidState state)
{
  uint8_t result = Ultrasonic_GetLastResult();
  GPIO_PinState blink = ((HAL_GetTick() / 250U) & 1U) != 0U
                      ? GPIO_PIN_SET : GPIO_PIN_RESET;

  /* 仅在 WAIT_SAFE 且确实收到超时/越界结果时提示故障；正常的测距预热、
     转弯和冷却不鸣叫，避免把“安全等待”误认为“检测到障碍”。 */
  if (state == ULTRASONIC_AVOID_WAIT_SAFE &&
      (result == ULTRASONIC_RESULT_TIMEOUT ||
       result == ULTRASONIC_RESULT_OUT_RANGE))
  {
    HAL_GPIO_WritePin(led1_GPIO_Port, led1_Pin,
                      result == ULTRASONIC_RESULT_TIMEOUT
                        ? blink : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(led2_GPIO_Port, led2_Pin,
                      result == ULTRASONIC_RESULT_OUT_RANGE
                        ? blink : GPIO_PIN_RESET);
  }
  else
  {
    HAL_GPIO_WritePin(led1_GPIO_Port, led1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(led2_GPIO_Port, led2_Pin, GPIO_PIN_RESET);
  }
}

static void update_ultrasonic_buzzer(UltrasonicAvoidState state)
{
  uint32_t now = HAL_GetTick();
  GPIO_PinState buzzer = GPIO_PIN_RESET;
  uint8_t safety_override = 0U;

  if (state == ULTRASONIC_AVOID_STOPPING ||
      state == ULTRASONIC_AVOID_BACKING ||
      state == ULTRASONIC_AVOID_GUARD ||
      state == ULTRASONIC_AVOID_TURNING)
  {
    safety_override = 1U;
    /* 正在脱困：100 ms 响、100 ms 停。 */
    buzzer = ((now / 100U) & 1U) == 0U
           ? GPIO_PIN_SET : GPIO_PIN_RESET;
  }
  /* Missing/noisy echoes and an unconfirmed distance are diagnostic states,
     not audible obstacle events. Leave manual phrase/audio ownership alone. */

  app_buzzer_safety_write(buzzer, safety_override);
}

static void motion_telemetry_task(void)
{
  uint32_t now = HAL_GetTick();
  UltrasonicMotionEstimate estimate;

  if (EXP7_VISION_ENABLED == 0U)
  {
    return;
  }

  UltrasonicMotion_Task(now);
  if (now - last_motion_telemetry_ms < 200U)
  {
    return;
  }

  last_motion_telemetry_ms = now;
  UltrasonicMotion_Get(&estimate);
  vision_uart_queue_motion_telemetry(estimate.valid,
                                     estimate.target_distance_mm,
                                     estimate.closing_speed_mm_s,
                                     estimate.relative_displacement_mm);
}

static void passive_ultrasonic_motion_task(void)
{
  uint32_t now = HAL_GetTick();
  uint16_t distance_cm = 0U;
  uint8_t result;

  /* KEY2 下只测距和估计，不调用避障状态机，因此不会接管电机。 */
  Ultrasonic_Task();
  if (!Ultrasonic_IsBusy() &&
      now - passive_measure_trigger_ms >= EXP7_PASSIVE_MEASURE_INTERVAL_MS)
  {
    if (Ultrasonic_Start() != 0U)
    {
      passive_measure_trigger_ms = now;
    }
  }

  result = Ultrasonic_GetResult(&distance_cm);
  if (result == ULTRASONIC_RESULT_OK)
  {
    (void)distance_cm;
    UltrasonicMotion_Update(Ultrasonic_GetLastDistanceMm(), now);
  }
  else if (result == ULTRASONIC_RESULT_TIMEOUT ||
           result == ULTRASONIC_RESULT_OUT_RANGE)
  {
    UltrasonicMotion_NoteInvalid(now);
  }
}

static void oled_application_task(AppMode mode)
{
  uint32_t now = HAL_GetTick();

  if (OledStatus_IsReady() != 0U && now - last_oled_update_ms >= 200U)
  {
    BatteryMonitorStatus battery;

    last_oled_update_ms = now;
    BatteryMonitor_Get(&battery);
    OledStatus_SetBattery(battery.millivolts,
                          battery.percent,
                          battery.valid,
                          battery.low);
    if (mode == APP_MODE_SIGN_LINE || mode == APP_MODE_SIGN_GYRO_TANGENT)
    {
      SignRouteStatus route_status;

      SignRoute_GetStatus(now, &route_status);
      OledStatus_SetSignLineData(
          mode == APP_MODE_SIGN_LINE ? 3U : 4U,
          sign_line_mask,
          sign_line_action,
          route_status.last_class,
          route_status.last_score,
          route_status.vision_online,
          (uint8_t)route_status.state,
          route_status.direction);
    }
    else
    {
      UltrasonicMotionEstimate estimate;

      UltrasonicMotion_Get(&estimate);
      OledStatus_SetData((uint8_t)mode,
                         estimate.valid,
                         estimate.target_distance_mm,
                         estimate.closing_speed_mm_s,
                         estimate.relative_displacement_mm);
    }
  }

  OledStatus_Task();
}

static void battery_telemetry_task(void)
{
  uint32_t now = HAL_GetTick();
  BatteryMonitorStatus battery;

  if (now - last_battery_uart_ms < 2000U)
  {
    return;
  }
  last_battery_uart_ms = now;
  BatteryMonitor_Get(&battery);
  DiagnosticUart_WriteString("BAT RAW=");
  DiagnosticUart_WriteUnsigned(battery.raw_adc);
  DiagnosticUart_WriteString(" MV=");
  DiagnosticUart_WriteUnsigned(battery.millivolts);
  DiagnosticUart_WriteString(" P=");
  DiagnosticUart_WriteUnsigned(battery.percent);
  DiagnosticUart_WriteString(" VALID=");
  DiagnosticUart_WriteUnsigned(battery.valid);
  DiagnosticUart_WriteString(" LOW=");
  DiagnosticUart_WriteUnsigned(battery.low);
  DiagnosticUart_WriteString("\r\n");
}

static void drive_base_telemetry_task(void)
{
  DriveBaseTelemetry telemetry;
  uint32_t now = HAL_GetTick();
  uint8_t motor;

  if (!tick_reached(now, last_drive_base_uart_ms + 1000U))
  {
    return;
  }
  last_drive_base_uart_ms = now;
  DriveBase_GetTelemetry(&telemetry);
  DiagnosticUart_WriteString("DRV M=");
  DiagnosticUart_WriteUnsigned((uint32_t)telemetry.mode);
  DiagnosticUart_WriteString(" P=");
  DiagnosticUart_WriteUnsigned((uint32_t)telemetry.position_state);
  DiagnosticUart_WriteString(" F=");
  DiagnosticUart_WriteUnsigned(telemetry.fault_mask);
  DiagnosticUart_WriteString(" V=");
  DiagnosticUart_WriteUnsigned(telemetry.battery_mv);
  DiagnosticUart_WriteString(" VC=");
  DiagnosticUart_WriteUnsigned(telemetry.voltage_compensation_permille);
  DiagnosticUart_WriteString(" SP=");
  DiagnosticUart_WriteSigned(telemetry.maximum_progress_spread_permille);
  DiagnosticUart_WriteString(" DEG=");
  DiagnosticUart_WriteUnsigned(DriveBase_GetLineDegradedMask());
  DiagnosticUart_WriteString(" LOG=");
  DiagnosticUart_WriteUnsigned(LineFaultLog_Count());
  for (motor = 0U; motor < DRIVE_BASE_WHEEL_COUNT; ++motor)
  {
    DiagnosticUart_WriteString(" W");
    DiagnosticUart_WriteUnsigned((uint32_t)motor + 1U);
    DiagnosticUart_WriteString("=");
    DiagnosticUart_WriteSigned(telemetry.controlled_cps[motor]);
    DiagnosticUart_WriteString("/");
    DiagnosticUart_WriteSigned(telemetry.measured_cps[motor]);
    DiagnosticUart_WriteString("/");
    DiagnosticUart_WriteSigned(telemetry.output_pwm[motor]);
    DiagnosticUart_WriteString(" Q=");
    DiagnosticUart_WriteSigned(telemetry.position_moved_counts[motor]);
    DiagnosticUart_WriteString("/");
    DiagnosticUart_WriteSigned(telemetry.position_target_counts[motor]);
    DiagnosticUart_WriteString(" I=");
    DiagnosticUart_WriteUnsigned(telemetry.illegal_transition_count[motor]);
    DiagnosticUart_WriteString(" D=");
    DiagnosticUart_WriteUnsigned(telemetry.direction_mismatch_count[motor]);
  }
  DiagnosticUart_WriteString("\r\n");
}

static uint8_t line_reading_mask(const LineTrackingReading *line)
{
  if (line == 0)
  {
    return 0U;
  }
  /* Display/control order is left outer, left inner, right inner, right outer:
     X2 X1 X3 X4. */
  return (uint8_t)((line->x2_black ? 8U : 0U) |
                   (line->x1_black ? 4U : 0U) |
                   (line->x3_black ? 2U : 0U) |
                   (line->x4_black ? 1U : 0U));
}

static void sign_line_telemetry_task(AppMode mode, const SignRouteStatus *route_status)
{
  VisionUartStats stats;
  DriveBaseTelemetry drive;
  uint32_t now = HAL_GetTick();

  DriveBase_GetTelemetry(&drive);
  SignTrace_Record(now, sign_line_mask, route_status, drive.requested_cps[0], drive.requested_cps[2]);

  if (!tick_reached(now, last_sign_uart_ms + 500U))
  {
    return;
  }
  last_sign_uart_ms = now;
  vision_uart_get_stats(&stats);
  DiagnosticUart_WriteString(mode == APP_MODE_SIGN_LINE ? "SIGN3" : "SIGN4");
  DiagnosticUart_WriteString(" LINE=");
  DiagnosticUart_WriteUnsigned(sign_line_mask);
  DiagnosticUart_WriteString(" A=");
  DiagnosticUart_WriteUnsigned(sign_line_action);
  DiagnosticUart_WriteString(" VIS=");
  DiagnosticUart_WriteSigned(route_status->last_class);
  DiagnosticUart_WriteString("/");
  DiagnosticUart_WriteUnsigned(route_status->last_score);
  DiagnosticUart_WriteString(" ON=");
  DiagnosticUart_WriteUnsigned(route_status->vision_online);
  DiagnosticUart_WriteString(" R=");
  DiagnosticUart_WriteUnsigned((uint32_t)route_status->state);
  DiagnosticUart_WriteString("/");
  DiagnosticUart_WriteSigned(route_status->direction);
  DiagnosticUart_WriteString(" SEQ=");
  DiagnosticUart_WriteUnsigned(route_status->last_sequence);
  DiagnosticUart_WriteString(" NAVF=");
  DiagnosticUart_WriteUnsigned(route_status->fault);
  DiagnosticUart_WriteString(" MM=");
  DiagnosticUart_WriteSigned(route_status->travel_mm);
  DiagnosticUart_WriteString(" YAW=");
  DiagnosticUart_WriteSigned(route_status->yaw_mdeg);
  DiagnosticUart_WriteString(" IMU=");
  DiagnosticUart_WriteUnsigned(route_status->yaw_valid);
  DiagnosticUart_WriteString(" P=");
  DiagnosticUart_WriteUnsigned((uint32_t)route_status->profile);
  DiagnosticUart_WriteString(route_status->approach_from_pause ? " REF=PAUSE" : " REF=PROBE");
  DiagnosticUart_WriteString(" H=");
  DiagnosticUart_WriteSigned(route_status->heading_error_mdeg);
  DiagnosticUart_WriteString(" SLOW=0 CAP=0"); /* retained diagnostic fields; no sign speed cap */
  DiagnosticUart_WriteString(" SEARCH=");
  DiagnosticUart_WriteUnsigned(route_status->searching);
  DiagnosticUart_WriteString(" BAD=");
  DiagnosticUart_WriteUnsigned(stats.bad_frames + stats.uart_errors +
                               stats.ring_overflows + stats.queue_overflows);
  DiagnosticUart_WriteString("\r\n");
}

/* Recognition and the explicit two-second observation stop; no moving-speed cap. */
static void sign_line_detection_task(AppMode mode)
{
  VisionDetection detection;
  if (mode != APP_MODE_SIGN_LINE && mode != APP_MODE_SIGN_GYRO_TANGENT)
  {
    SignObservation_Reset();
    DriveBase_SetSpeedLimitCps(0L);
    return;
  }
  while (vision_uart_take_detection(&detection) != 0U)
  {
    SignRouteStatus observation_route;
    SignRoute_GetStatus(HAL_GetTick(), &observation_route);
    SignObservation_AllowPause(observation_route.direction == 0 &&
        (observation_route.state == SIGN_ROUTE_IDLE || observation_route.state == SIGN_ROUTE_ARMED ||
         observation_route.state == SIGN_ROUTE_PROBE || observation_route.state == SIGN_ROUTE_WAIT_SIGN));
    SignObservation_ObserveDetection(&detection, HAL_GetTick());
    SignRoute_ObserveDetection(&detection);
    if (SignHorn_Observe(&detection, HAL_GetTick()))
      (void)BuzzerPhrase400_Start(5U);
  }
}

static void sign_rgb_off(void)
{
  /* Left green is PE7, not PG7; use each LED's actual port. */
  HAL_GPIO_WritePin(LRGB_R_GPIO_Port, LRGB_R_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LRGB_G_GPIO_Port, LRGB_G_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LRGB_B_GPIO_Port, LRGB_B_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(RRGB_R_GPIO_Port, RRGB_R_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(RRGB_G_GPIO_Port, RRGB_G_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(RRGB_B_GPIO_Port, RRGB_B_Pin, GPIO_PIN_RESET);
}

static void sign_line_task(AppMode mode)
{
  SignRouteCommand route_command;
  MpuYawReading yaw;
  SignRouteStatus route_status;
  WheelEncoderCounts counts;
  LineTrackingReading line;
  uint32_t now;
  uint8_t observation_paused;

  SignRoute_SetProfile(mode == APP_MODE_SIGN_GYRO_TANGENT ?
      SIGN_ROUTE_PROFILE_GYRO_TANGENT : SIGN_ROUTE_PROFILE_STANDARD);
  /* Diagnostics/display may have run after the background service. Consume
     available FIFO history immediately before this angle-dependent decision. */
  MpuYaw_Refresh(HAL_GetTick());
  line = line_tracking_read();
  now = HAL_GetTick();
  sign_line_mask = line_reading_mask(&line);
  /* Keep sampling the real line even while a route preference is active. */
  WheelEncoder_GetCounts(&counts);
  SignRoute_UpdateEncoders(counts.motor1, counts.motor2, counts.motor3, counts.motor4);
  MpuYaw_GetReading(&yaw);
  SignRoute_UpdateYaw(yaw.yaw_mdeg, MpuYaw_IsReady(now));
  SimpleLine_UpdateYaw(&sign_line_controller.guard, yaw.yaw_mdeg, MpuYaw_IsReady(now), yaw.generation);
  if (mode==APP_MODE_SIGN_LINE)
  {
    SignObservation_UpdateLine(sign_line_mask,now);
    if (SignObservation_SeekingLine()) SignRoute_MarkObservationSearch();
  }
  observation_paused = SignObservation_Paused(now);
  SignRoute_UpdateObservationPause(mode==APP_MODE_SIGN_LINE ?
      SignObservation_HoldingRoute(now) : observation_paused, now);
  SignRoute_Step(sign_line_mask, now, &route_command);
  SignRoute_GetStatus(now, &route_status);
  sign_line_action = SignLineFollow_Step(&sign_line_controller, &line, EXP7_LINE_SPEED,
      &route_status, &route_command, observation_paused);
  if (sign_line_action == 5U) UltrasonicMotion_Reset();

  SignRoute_GetStatus(now, &route_status);
  app_buzzer_safety_write(GPIO_PIN_RESET, 0U);
  sign_rgb_off();
  HAL_GPIO_WritePin(led1_GPIO_Port, led1_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(led2_GPIO_Port, led2_Pin,
                    route_status.vision_online != 0U ?
                    GPIO_PIN_SET : GPIO_PIN_RESET);
  sign_line_telemetry_task(mode, &route_status);
}

static void vision_line_v4_diagnostic_dump(void)
{
  VisionLineV4Reading reading = vision_uart_get_line_v4();
  VisionUartStats stats;
  uint32_t now = HAL_GetTick();

  vision_uart_get_stats(&stats);
  DiagnosticUart_WriteString("VLINK V4=");
  DiagnosticUart_WriteUnsigned(stats.line_v4_frames);
  DiagnosticUart_WriteString(" SIGN=");
  DiagnosticUart_WriteUnsigned(stats.valid_frames + stats.none_frames);
  DiagnosticUart_WriteString(" BAD=");
  DiagnosticUart_WriteUnsigned(stats.bad_frames + stats.uart_errors +
                               stats.ring_overflows +
                               stats.queue_overflows);
  DiagnosticUart_WriteString(" F=");
  DiagnosticUart_WriteUnsigned(reading.frame_valid);
  DiagnosticUart_WriteString(" AGE=");
  DiagnosticUart_WriteUnsigned(reading.frame_valid != 0U ?
                               now - reading.received_ms : 0U);
  DiagnosticUart_WriteString(" L=");
  DiagnosticUart_WriteUnsigned(reading.line_found);
  DiagnosticUart_WriteString(" O=");
  DiagnosticUart_WriteSigned(reading.offset);
  DiagnosticUart_WriteString(" A=");
  DiagnosticUart_WriteSigned(reading.angle);
  DiagnosticUart_WriteString(" BOT=");
  DiagnosticUart_WriteSigned(reading.bottom);
  DiagnosticUart_WriteString(" OBS=");
  DiagnosticUart_WriteUnsigned(reading.obstacle_found);
  DiagnosticUart_WriteString(" AY=");
  DiagnosticUart_WriteUnsigned(reading.obstacle_bottom);
  DiagnosticUart_WriteString("\r\n");
}

static AppMode read_requested_mode(AppMode current_mode)
{
  uint8_t remote_key = IrRemote_TakeVirtualKey();
  uint8_t serial_key = app_take_serial_virtual_key();

  /* 遥控数字 0 为最高优先级停车；数字 1/2/3 与实体键等效，
     数字 4 为陀螺仪切线圆弧，数字 5 为 K210 v4 视觉循线。 */
  if (remote_key == IR_REMOTE_VIRTUAL_STOP ||
      serial_key == IR_REMOTE_VIRTUAL_STOP)
  {
    DfPlayerMini_Stop();
    BuzzerPhrase400_Stop();
    return APP_MODE_STOPPED;
  }

  /* Direction-pad audio controls never change the drive mode.  Centre toggles
     play/pause, left/right select the previous/next physical file regardless
     of its name, and up/down change volume.  NEC repeats stay suppressed. */
  if (remote_key == IR_REMOTE_VIRTUAL_AUDIO_TOGGLE)
  {
    app_audio_toggle();
  }
  else if (remote_key == IR_REMOTE_VIRTUAL_AUDIO_NEXT)
  {
    app_audio_next();
  }
  else if (remote_key == IR_REMOTE_VIRTUAL_AUDIO_PREVIOUS)
  {
    app_audio_previous();
  }
  else if (remote_key == IR_REMOTE_VIRTUAL_AUDIO_VOLUME_UP)
  {
    app_audio_adjust_volume(EXP7_AUDIO_VOLUME_STEP);
  }
  else if (remote_key == IR_REMOTE_VIRTUAL_AUDIO_VOLUME_DOWN)
  {
    app_audio_adjust_volume(-EXP7_AUDIO_VOLUME_STEP);
  }

  if (HAL_GPIO_ReadPin(key1_GPIO_Port, key1_Pin) == GPIO_PIN_RESET ||
      remote_key == IR_REMOTE_VIRTUAL_KEY1)
  {
    return APP_MODE_INTEGRATED;
  }
  if (serial_key == IR_REMOTE_VIRTUAL_KEY1)
  {
    return APP_MODE_INTEGRATED;
  }
  if (HAL_GPIO_ReadPin(key2_GPIO_Port, key2_Pin) == GPIO_PIN_RESET ||
      remote_key == IR_REMOTE_VIRTUAL_KEY2)
  {
    return APP_MODE_LINE_ONLY;
  }
  if (serial_key == IR_REMOTE_VIRTUAL_KEY2)
  {
    return APP_MODE_LINE_ONLY;
  }
  if (HAL_GPIO_ReadPin(key3_GPIO_Port, key3_Pin) == GPIO_PIN_RESET ||
      remote_key == IR_REMOTE_VIRTUAL_KEY3)
  {
    return APP_MODE_SIGN_LINE;
  }
  if (serial_key == IR_REMOTE_VIRTUAL_KEY3)
  {
    return APP_MODE_SIGN_LINE;
  }
  if (remote_key == IR_REMOTE_VIRTUAL_KEY4 ||
      serial_key == IR_REMOTE_VIRTUAL_KEY4)
  {
    return APP_MODE_SIGN_GYRO_TANGENT;
  }
  if (remote_key == IR_REMOTE_VIRTUAL_KEY5 ||
      serial_key == IR_REMOTE_VIRTUAL_KEY5)
  {
    return APP_MODE_FIXED_BYPASS;
  }

  return current_mode;
}

static void cancel_vision_action(void)
{
  vision_action = VISION_ACTION_NONE;
  vision_action_deadline = 0U;
}

static void start_vision_command(VisionCommand command, uint32_t now)
{
  if (vision_action == VISION_ACTION_HORN && command != VISION_CMD_HORN)
  {
    BuzzerPhrase400_Stop();
  }

  switch (command)
  {
    case VISION_CMD_LIMIT_SPEED:
      line_speed = EXP7_LIMIT_SPEED;
      break;

    case VISION_CMD_FREE_SPEED:
      line_speed = EXP7_LINE_SPEED;
      break;

    case VISION_CMD_RIGHT:
      vision_action = VISION_ACTION_TURN_RIGHT;
      vision_action_deadline = now + EXP7_VISION_TURN_TIME_MS;
      break;

    case VISION_CMD_LEFT:
      vision_action = VISION_ACTION_TURN_LEFT;
      vision_action_deadline = now + EXP7_VISION_TURN_TIME_MS;
      break;

    case VISION_CMD_GARAGE_ONE:
      vision_action = VISION_ACTION_GARAGE_ONE;
      vision_action_deadline = now + EXP7_GARAGE_TIME_MS;
      break;

    case VISION_CMD_GARAGE_TWO:
      vision_action = VISION_ACTION_GARAGE_TWO;
      vision_action_deadline = now + EXP7_GARAGE_TIME_MS;
      break;

    case VISION_CMD_HORN:
      vision_action = VISION_ACTION_HORN;
      vision_action_deadline = now + EXP7_HORN_TIMEOUT_MS;
      (void)BuzzerPhrase400_Start(1U);
      break;

    case VISION_CMD_STOP:
      DfPlayerMini_Stop();
      BuzzerPhrase400_Stop();
      vision_action = VISION_ACTION_STOP;
      vision_action_deadline = now + EXP7_STOP_HOLD_MS;
      break;

    /* 这些标志用于离开/恢复黑线，当前课程版本不需要额外动作。 */
    case VISION_CMD_GREEN_LIGHT:
    case VISION_CMD_SCHOOL:
    case VISION_CMD_WALK:
    case VISION_CMD_CHUKU_TRACK_LINE:
    case VISION_CMD_NONE:
    default:
      break;
  }
}

static uint8_t run_vision_action(uint32_t now)
{
  if (vision_action == VISION_ACTION_NONE)
  {
    return 0U;
  }

  if (tick_reached(now, vision_action_deadline))
  {
    if (vision_action == VISION_ACTION_HORN)
    {
      BuzzerPhrase400_Stop();
    }
    cancel_vision_action();
    advanced_stop();
    return 0U;
  }

  switch (vision_action)
  {
    case VISION_ACTION_TURN_LEFT:
      UltrasonicMotion_Reset();
      advanced_turn_left(EXP7_VISION_TURN_INNER_SPEED,
                         EXP7_VISION_TURN_OUTER_SPEED);
      break;

    case VISION_ACTION_TURN_RIGHT:
      UltrasonicMotion_Reset();
      advanced_turn_right(EXP7_VISION_TURN_INNER_SPEED,
                          EXP7_VISION_TURN_OUTER_SPEED);
      break;

    case VISION_ACTION_GARAGE_ONE:
    case VISION_ACTION_GARAGE_TWO:
      /* 视觉识别到 1/2 号库后按指导书倒车入库约 2 秒。 */
      advanced_drive_backward(EXP7_GARAGE_SPEED, EXP7_GARAGE_SPEED);
      break;

    case VISION_ACTION_HORN:
      /* 新音频为非阻塞五段节奏；播放期间车辆保持停止。 */
      advanced_stop();
      if (BuzzerPhrase400_IsPlaying() == 0U)
      {
        cancel_vision_action();
        return 0U;
      }
      break;

    case VISION_ACTION_STOP:
      advanced_stop();
      /* 两灯常亮表示停车来自视觉指令，便于与红外蜂鸣和超声波
         WAIT_SAFE 闪灯区分。 */
      HAL_GPIO_WritePin(led1_GPIO_Port, led1_Pin, GPIO_PIN_SET);
      HAL_GPIO_WritePin(led2_GPIO_Port, led2_Pin, GPIO_PIN_SET);
      break;

    case VISION_ACTION_NONE:
    default:
      cancel_vision_action();
      app_buzzer_safety_write(GPIO_PIN_RESET, 0U);
      advanced_stop();
      return 0U;
  }

  return 1U;
}

static void experiment7_integrated_once(void)
{
  VisionCommand command = VISION_CMD_NONE;
  uint32_t now;

  if (EXP7_VISION_ENABLED != 0U)
  {
    command = vision_uart_take_event();
  }
  now = HAL_GetTick();
  if (command != VISION_CMD_NONE)
  {
    start_vision_command(command, now);
  }

  /* 红外和超声波触发已在调用本函数前统一交给 V2 绕障控制器。
     这里不再保留第二套会抢占电机的旧红外动作状态机。 */

  /* 优先级 2：视觉转弯、鸣笛和入库动作。 */
  if (EXP7_VISION_ENABLED != 0U && run_vision_action(now))
  {
    return;
  }

  line_tracking_set_fast_follow(fixed_bypass_mode);
  /* 优先级 3：无视觉动作时进行四路黑线闭环循迹。 */
  line_tracking_set_straight_boost(1U);
  {
    LineTrackingAction action = line_tracking_follow_once(line_speed,
                                                        ultrasonic_forward_speed_limit);

    /* 差速转弯时声束不再稳定指向同一墙面，相对运动估计作废。 */
    if (action != LINE_ACTION_FORWARD && action != LINE_ACTION_CROSSING)
    {
      UltrasonicMotion_Reset();
    }
  }
}

/* A running line mode may wait, but no automatic owner may park forever.
   Called before fault/ultrasonic/bypass early returns, after operator mode
   changes. The timeout recovery issues rotation only, then retries owners. */
static uint8_t service_bounded_line_wait(AppMode mode)
{
  DriveBaseTelemetry telemetry;
  LineWaitAction action;
  uint32_t now = HAL_GetTick();
  uint8_t enabled = mode != APP_MODE_STOPPED &&
      mode != APP_MODE_SIGN_LINE && mode != APP_MODE_SIGN_GYRO_TANGENT;
  uint8_t paused;
  DriveBase_GetTelemetry(&telemetry);
  paused = telemetry.mode == DRIVE_BASE_STOPPED || telemetry.mode == DRIVE_BASE_BRAKING ||
      telemetry.mode == DRIVE_BASE_FAULT || telemetry.fault_mask != 0U;
  action = LineWaitGuard_Update(&line_wait_guard, enabled, paused, now);
  if (action == LINE_WAIT_RECOVERING)
  {
    LineTrackingReading line = line_tracking_read();
    uint8_t mask = line_reading_mask(&line);
    /* A real middle contact can return control before the recovery deadline.
       Wide/transverse and sole-outer contacts do not finish recovery. */
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
    if ((mode == APP_MODE_INTEGRATED || mode == APP_MODE_FIXED_BYPASS))
    {
      infrared = ir_avoid_read();
    }
    if ((mode == APP_MODE_INTEGRATED || mode == APP_MODE_FIXED_BYPASS) &&
        (infrared.left_obstacle || infrared.right_obstacle))
      line_wait_side = choose_bypass_direction(&infrared);
    else if (line_side) line_wait_side = line_side;
    if (line_wait_side == 0) line_wait_side = -1;
    record.time_ms = now; record.source = LINE_SEARCH_WAIT_RECOVERY;
    record.chosen_side = line_wait_side;
    record.edge_age_ms = record.wide_age_ms = UINT32_MAX;
    record.drive_fault = telemetry.fault_mask;
    record.bypass_fault = LineObstacleBypass_GetFaultMask();
    MpuYaw_GetReading(&imu);
    record.gyro_fault = GyroTurn_GetFault(); record.imu_fault = imu.fault;
    record.pause_reason = record.drive_fault ? 1U : (record.bypass_fault ? 2U :
        (((mode == APP_MODE_INTEGRATED || mode == APP_MODE_FIXED_BYPASS) && UltrasonicAvoid_GetState() != ULTRASONIC_AVOID_FORWARD) ? 3U : 4U));
    LineFaultLog_RecordSearch(&record); /* Preserve reason before clearing it. */
    WheelSpeedObserver_Stop();
    LineObstacleBypass_Stop();
    DriveBase_Stop(DRIVE_STOP_COAST); /* ClearFault deliberately rejects active braking. */
    DriveBase_ClearFault();
    LineBypassTurn_Recover();
    DriveBase_SetSpeedLimitCps(0L);
    SignRoute_Reset();
    cancel_vision_action();
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
    if ((mode == APP_MODE_INTEGRATED || mode == APP_MODE_FIXED_BYPASS))
    {
      bypass_rearm_pending = 1U; bypass_ir_clear_samples = 0U;
      bypass_ir_trigger_candidate = 0U;
      bypass_rearm_not_before_ms = now + BYPASS_REARM_DELAY_MS;
      configure_ultrasonic_avoid();
    }
    WheelSpeedObserver_Start();
    last_fast_speed_cps = 0U; last_fast_speed_ms = now;
  }
  return 0U;
}
int main(void)
{
  AppMode app_mode = APP_MODE_STOPPED;
  LineObstacleBypassConfig bypass_config;

  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  BuzzerPhrase400_Init();
  DiagnosticUart_Init();
  AudioResumeStore_Init();
  DfPlayerMini_Init();
  (void)DfPlayerMini_SetResumeTrack(AudioResumeStore_GetTrack());
  (void)DfPlayerMini_SetLoopCurrent(1U);
  audio_store_error_reported = 0U;
  DiagnosticUart_WriteString("DFPLAYER UART4 PC10/PC11: CENTER=PLAY/PAUSE LEFT=PREVIOUS RIGHT=NEXT UP/DOWN=VOL+/-2 LOOP=CURRENT REMEMBER=");
  DiagnosticUart_WriteUnsigned(AudioResumeStore_GetTrack());
  DiagnosticUart_WriteString("\r\n");
  DiagnosticUart_WriteString("\r\nLINE FAULT LOG v1: f=DUMP WHEN STOPPED; RAM ONLY; KEEP POWER ON; DEG=FEEDFORWARD WHEEL MASK\r\n");
  DiagnosticUart_WriteString("\r\nEXP7 UNIFIED MOTION V1 READY: DEFAULT STOP; 1=LINE+BYPASS 2=LINE 3=SIGN+RING 4=SIGN+GYRO-ARC 5=K210-VLINE4 0=STOP\r\n");
  motor_pwm_init();
  WheelEncoder_Init();
  WheelSpeedObserver_Init();
  WheelSpeedControl_Init();
  Figure8Encoder_Init();
  EncoderLinear_Init();
  EncoderStraight_Init();
  EncoderTurn_Init();
  SquareEncoder_Init();
  LineObstacleBypass_GetDefaultConfig(&bypass_config);
  bypass_config.fixed_route_direction = 1; /* Fixed rectangle: right 30 cm, ahead 36 cm, inward 45 deg. */
  bypass_config.infrared_enabled = EXP7_IR_AVOID_ENABLED;
  bypass_config.emergency_speed_cps = EXP7_EMERGENCY_BRAKE_SPEED_CPS;
  bypass_config.reverse_cps = EXP7_BYPASS_REVERSE_CPS;
  bypass_config.forward_cps = EXP7_BYPASS_FORWARD_CPS;
  bypass_config.clear_probe_cps = EXP7_BYPASS_CLEAR_PROBE_CPS;
  bypass_config.return_cps = EXP7_BYPASS_RETURN_CPS;
  bypass_config.turn_cps = EXP7_BYPASS_TURN_CPS;
  LineObstacleBypass_Init(&bypass_config);
  ir_avoid_init();
  if (!EXP7_IR_AVOID_ENABLED)
  {
    ir_avoid_set_enabled(false);
    /* PE5/PE6 emitters are active low. Keep shared ADC3 initialization for
       BatteryMonitor, but physically disable the obstacle IR emitters. */
    HAL_GPIO_WritePin(IR_LEFT_ENABLE_GPIO_Port, IR_LEFT_ENABLE_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(IR_RIGHT_ENABLE_GPIO_Port, IR_RIGHT_ENABLE_Pin, GPIO_PIN_SET);
  }
  BatteryMonitor_Init();
  DriveBase_Init();
  line_tracking_init();
  SignLineFollow_Init(&sign_line_controller);
  SignRoute_Init();
  SignTrace_Init();
  SignHorn_Reset();
  SignObservation_Reset();  vision_uart_init();
  Ultrasonic_Init();
  IrRemote_Init();
  configure_ultrasonic_avoid();
  advanced_stop();

  line_speed = EXP7_LINE_SPEED;
  cancel_vision_action();
  ultrasonic_forward_speed_limit = 0;
  last_motion_telemetry_ms = HAL_GetTick();
  passive_measure_trigger_ms = HAL_GetTick() -
                               EXP7_PASSIVE_MEASURE_INTERVAL_MS;
  last_oled_update_ms = HAL_GetTick() - 200U;
  last_sign_uart_ms = HAL_GetTick() - 500U;
  last_bypass_uart_ms = HAL_GetTick() - 200U;
  last_battery_uart_ms = HAL_GetTick() - 2000U;
  last_drive_base_uart_ms = HAL_GetTick() - 1000U;
  last_fast_speed_cps = 0U;
  last_fast_speed_ms = HAL_GetTick();
  next_bypass_direction = 1;
  bypass_rearm_pending = 0U;
  bypass_ir_clear_samples = 0U;
  bypass_rearm_not_before_ms = 0U;
  bypass_ir_trigger_candidate = 0U;
  bypass_ir_trigger_since_ms = HAL_GetTick();
  sign_line_mask = 0U;
  sign_line_action = 0U;

  /* 红外发射管和 ADC 先预热；实验五原版也保留约 1 s，避免刚上电
     读数还未稳定就把基线判成无效，导致红外整段不参与避障。 */
  if (EXP7_IR_AVOID_ENABLED) HAL_Delay(1000U);

  /* 标定期间车头前方保持无遮挡，分别建立左右红外基线。 */
  if (EXP7_IR_AVOID_ENABLED && !ir_avoid_calibrate())
  {
    /* 红外标定失败时不再无限卡在这里；读数被屏蔽，车辆仍可由
       超声波（包括显式启用的开阔区降级）控制。复位后会重新尝试标定。 */
    ir_avoid_set_enabled(false);
  }

  OledStatus_Init();
  MpuYaw_Init(HAL_GetTick());
  DiagnosticUart_WriteString("IMU: KEEP STILL 2s; g=STATUS c=RECALIBRATE IN STOP; KEY1 REQUIRES READY\r\n");

  /* 默认锁存停车；按1/2/3/4/5后启动对应功能。 */
  line_tracking_set_no_line_forward(0U);
  line_tracking_reset();

#if defined(LINE_TRACKING_LIFT_TEST)
  LineTrackingLiftTest_Run();
#endif

  while (1)
  {
    UltrasonicAvoidState ultrasonic_state;
    IrAvoidReading ir_status = {0};
    uint32_t approach_speed_cps = 0U;
    uint16_t emergency_distance_cm = EXP7_ULTRASONIC_STOP_CM;
    AppMode requested_mode = read_requested_mode(app_mode);

    DfPlayerMini_Task(HAL_GetTick());
    app_audio_persist_task((app_mode == APP_MODE_STOPPED &&
                            requested_mode == APP_MODE_STOPPED) ? 1U : 0U);

    /* The phrase deadlines are absolute, so this 1 ms main-loop service does
       not accumulate timing drift. */
    BuzzerPhrase400_Task(HAL_GetTick());

    {
      DriveBaseTelemetry drive;
      uint8_t stationary;
      unsigned wheel;
      DriveBase_GetTelemetry(&drive);
      stationary = app_mode == APP_MODE_STOPPED && drive.mode == DRIVE_BASE_STOPPED;
      for (wheel = 0; wheel < DRIVE_BASE_WHEEL_COUNT; ++wheel)
        if (drive.requested_cps[wheel] || drive.measured_cps[wheel] > 30 ||
            drive.measured_cps[wheel] < -30 || drive.output_pwm[wheel]) stationary = 0U;
      if (imu_calibrate_requested)
      {
        imu_calibrate_requested = 0U;
        if (stationary && requested_mode == APP_MODE_STOPPED && GyroTurn_ClearFault())
        {
          MpuYaw_Init(HAL_GetTick());
          DiagnosticUart_WriteString("IMU CAL START: KEEP STILL\r\n");
        }
        else DiagnosticUart_WriteString("IMU CAL REJECTED: STOP FIRST\r\n");
      }
      MpuYaw_Task(HAL_GetTick(), stationary);
      {
        MpuYawReading imu;
        MpuYaw_GetReading(&imu);
        if (imu.generation != imu_generation)
        {
          imu_generation = imu.generation;
          SignRoute_Reset(); /* Never span an unobserved yaw interval. */
        }
      }
      /* In operator STOP, acknowledge a transient fault without any motion.
         Active modes use the separately bounded automatic recovery below. */
      if (stationary) (void)GyroTurn_ClearTransientFault();
      /* Sensor availability never rewrites the operator's requested mode.
         Bypass selects a usable endpoint source at its next action boundary. */
      if (imu_dump_requested && app_mode == APP_MODE_STOPPED &&
          requested_mode == APP_MODE_STOPPED)
      {
        MpuYawReading imu;
        MpuYaw_GetReading(&imu);
        imu_dump_requested = 0U;
        DiagnosticUart_WriteString("IMU S="); DiagnosticUart_WriteUnsigned(imu.state);
        DiagnosticUart_WriteString(" F="); DiagnosticUart_WriteUnsigned(imu.fault);
        DiagnosticUart_WriteString(" CAL="); DiagnosticUart_WriteUnsigned(imu.calibration_samples);
        DiagnosticUart_WriteString(" REJECT="); DiagnosticUart_WriteUnsigned(imu.cal_reject);
        DiagnosticUart_WriteString(" LAST_REJECT="); DiagnosticUart_WriteUnsigned(imu.cal_last_reject);
        DiagnosticUart_WriteString(" REJECTS="); DiagnosticUart_WriteUnsigned(imu.cal_rejections);
        DiagnosticUart_WriteString(" ACC_WARN="); DiagnosticUart_WriteUnsigned(imu.accel_reference_warning);
        DiagnosticUart_WriteString(" ACC_REF=");
        for (unsigned axis=0; axis<3; ++axis) {
          if (axis) DiagnosticUart_WriteString(",");
          DiagnosticUart_WriteSigned(imu.cal_accel_mean[axis]);
        }
        DiagnosticUart_WriteString(" ACC=");
        for (unsigned axis=0; axis<3; ++axis) {
          if (axis) DiagnosticUart_WriteString(",");
          DiagnosticUart_WriteSigned(imu.raw_accel[axis]);
        }
        DiagnosticUart_WriteString(" GRAW=");
        for (unsigned axis=0; axis<3; ++axis) {
          if (axis) DiagnosticUart_WriteString(",");
          DiagnosticUart_WriteSigned(imu.raw_gyro[axis]);
        }
        DiagnosticUart_WriteString(" BIAS_MRAW="); DiagnosticUart_WriteSigned(imu.bias_milliraw);
        DiagnosticUart_WriteString(" YAW_MDEG=");
        DiagnosticUart_WriteSigned((int32_t)(imu.yaw_mdeg % 360000));
        DiagnosticUart_WriteString(" RATE_MDEG_S="); DiagnosticUart_WriteSigned(imu.rate_mdeg_s);
        DiagnosticUart_WriteString(" AGE="); DiagnosticUart_WriteUnsigned(HAL_GetTick() - imu.last_sample_ms);
        DiagnosticUart_WriteString(" PENDING="); DiagnosticUart_WriteUnsigned(imu.pending_frames);
        DiagnosticUart_WriteString(" FIFO_PEAK="); DiagnosticUart_WriteUnsigned(imu.peak_fifo_bytes);
        DiagnosticUart_WriteString(" GAP_MAX="); DiagnosticUart_WriteUnsigned(imu.max_service_gap_ms);
        DiagnosticUart_WriteString(" BACKLOG="); DiagnosticUart_WriteUnsigned(imu.backlog_events);
        DiagnosticUart_WriteString(" RESTARTS="); DiagnosticUart_WriteUnsigned(imu.restart_count);
        DiagnosticUart_WriteString(" LAST_F="); DiagnosticUart_WriteUnsigned(imu.last_fault);
        DiagnosticUart_WriteString(" GENERATION="); DiagnosticUart_WriteUnsigned(imu.generation);
        DiagnosticUart_WriteString(" TURN_F="); DiagnosticUart_WriteUnsigned(GyroTurn_GetFault());
        DiagnosticUart_WriteString(" TURN_MDEG="); DiagnosticUart_WriteSigned(GyroTurn_GetAchievedAngleMdeg());
        DiagnosticUart_WriteString("\r\n");
      }
    }

    if (requested_mode != app_mode)
    {
      uint32_t discarded_black_ms;
      (void)LineSensorSample_TakeAllBlack(&discarded_black_ms);
      SignHorn_Reset();
      SignObservation_Reset();
      DriveBase_SetSpeedLimitCps(0L);
      LineWaitGuard_Reset(&line_wait_guard);
      WheelSpeedObserver_Stop();
      LineObstacleBypass_Stop();
      LineBypassRange_Reset();
      bypass_rearm_pending = 0U;
      bypass_ir_clear_samples = 0U;
      bypass_rearm_not_before_ms = 0U;
      bypass_ir_trigger_candidate = 0U;
      bypass_ir_trigger_since_ms = HAL_GetTick();
      last_fast_speed_cps = 0U;
      last_fast_speed_ms = HAL_GetTick();
      SignLineFollow_Stop(&sign_line_controller);
      SignRoute_Reset();
      vision_uart_reset_detections();
      vision_uart_reset_line_v4();

      advanced_stop();
      DriveBase_ClearFault();
      DriveBase_SetLineFaultObservation(0U, 0U, 0U);
      advanced_set_forward_speed_limit(MOTOR_PWM_PERIOD);
      cancel_vision_action();
      line_speed = EXP7_LINE_SPEED;
      line_tracking_reset();
      DfPlayerMini_Stop();
      BuzzerPhrase400_Stop();
      HAL_GPIO_WritePin(led1_GPIO_Port, led1_Pin, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(led2_GPIO_Port, led2_Pin, GPIO_PIN_RESET);

      app_mode = requested_mode;
      fixed_bypass_mode = (app_mode == APP_MODE_FIXED_BYPASS);
      if ((app_mode == APP_MODE_INTEGRATED || app_mode == APP_MODE_FIXED_BYPASS))
      {
        /* Use the same current tracking profile as KEY2; obstacle ownership
           and ultrasonic speed caps remain in the integrated-mode wrapper. */
        line_tracking_start_following();
        /* 切回综合模式后从 WAIT_SAFE 重新确认距离，不沿用上一次动作。 */
        configure_ultrasonic_avoid();
        WheelSpeedObserver_Start();
        last_fast_speed_cps = 0U;
        last_fast_speed_ms = HAL_GetTick();
      }
      else if (app_mode == APP_MODE_LINE_ONLY)
      {
        line_tracking_start_following();
        UltrasonicMotion_Reset();
        passive_measure_trigger_ms = HAL_GetTick() -
                                     EXP7_PASSIVE_MEASURE_INTERVAL_MS;
        WheelSpeedObserver_Start();
      }
      else if (app_mode == APP_MODE_SIGN_LINE)
      {
        SignTrace_Init(); /* New operator run records this attempt, not a previous frozen fault. */
        SignLineFollow_Start(&sign_line_controller);
        UltrasonicMotion_Reset();
        ultrasonic_forward_speed_limit = 0;
        SignRoute_Reset();
        SignRoute_SetProfile(SIGN_ROUTE_PROFILE_STANDARD);
        vision_uart_reset_detections();
        last_sign_uart_ms = HAL_GetTick() - 500U;
        DiagnosticUart_WriteString("SIGN3 KEY2 RING NAV START\r\n");
      }
      else if (app_mode == APP_MODE_SIGN_GYRO_TANGENT)
      {
        SignLineFollow_Start(&sign_line_controller);
        UltrasonicMotion_Reset();
        ultrasonic_forward_speed_limit = 0;
        SignRoute_Reset();
        SignRoute_SetProfile(SIGN_ROUTE_PROFILE_GYRO_TANGENT);
        vision_uart_reset_detections();
        last_sign_uart_ms = HAL_GetTick() - 500U;
        DiagnosticUart_WriteString("SIGN4 GYRO TANGENT ARC START\r\n");
      }
      else
      {
        /* 数字 0 的锁存停车态：停止所有动作，等待 1/2/3/4/5 恢复。 */
        line_tracking_set_no_line_forward(0U);
        line_tracking_set_smooth_mode(0U);
        line_tracking_set_turn_gain_percent(100U);
        UltrasonicMotion_Reset();
        ultrasonic_forward_speed_limit = 0;
        advanced_stop();
      }

      if (app_mode == APP_MODE_SIGN_LINE ||
          app_mode == APP_MODE_SIGN_GYRO_TANGENT ||
          app_mode == APP_MODE_FIXED_BYPASS)
      {
        sign_rgb_off();
      }
    }

    if (app_mode == APP_MODE_SIGN_LINE || app_mode == APP_MODE_SIGN_GYRO_TANGENT)
      sign_rgb_off(); /* before recovery/fault paths can return early */
    sign_line_detection_task(app_mode);
    DriveBase_Task(HAL_GetTick());
    drive_base_telemetry_task();
    LineFaultLog_Task((uint8_t)(app_mode == APP_MODE_STOPPED));
    SignTrace_Task((uint8_t)(app_mode == APP_MODE_STOPPED));
    if (service_bounded_line_wait(app_mode))
    {
      HAL_Delay(1U);
      continue;
    }

    if (DriveBase_GetFaultMask() != 0U &&
        ((app_mode == APP_MODE_INTEGRATED || app_mode == APP_MODE_FIXED_BYPASS) ||
         app_mode == APP_MODE_LINE_ONLY ||
         app_mode == APP_MODE_SIGN_LINE ||
         app_mode == APP_MODE_SIGN_GYRO_TANGENT) &&
        LineObstacleBypass_GetState() == LINE_BYPASS_IDLE)
    {
      uint8_t fault_code = encoder_fault_beep_code(
          DriveBase_GetFaultMask());
      uint32_t phase = HAL_GetTick() % 2500U;

      app_buzzer_safety_write(
          (phase < (uint32_t)fault_code * 250U &&
           (phase % 250U) < 100U) ? GPIO_PIN_SET : GPIO_PIN_RESET,
          1U);
      if (app_mode != APP_MODE_SIGN_LINE && app_mode != APP_MODE_SIGN_GYRO_TANGENT)
        HAL_GPIO_WritePin(LRGB_R_GPIO_Port, LRGB_R_Pin, GPIO_PIN_SET);
      HAL_GPIO_WritePin(led1_GPIO_Port, led1_Pin, GPIO_PIN_SET);
      HAL_GPIO_WritePin(led2_GPIO_Port, led2_Pin, GPIO_PIN_SET);
      HAL_Delay(1U);
      continue;
    }

    if (EXP7_IR_AVOID_ENABLED &&
        ((app_mode == APP_MODE_INTEGRATED || app_mode == APP_MODE_FIXED_BYPASS) || app_mode == APP_MODE_LINE_ONLY))
    {
      /* KEY1/KEY2 保留红外状态灯；模式 3/4 关闭 RGB，避免照射标志。 */
      ir_status = ir_avoid_read();
      ir_avoid_show_status(&ir_status);
    }
    vision_uart_poll();

    BatteryMonitor_Task();
    battery_telemetry_task();
    BuzzerPhrase400_Task(HAL_GetTick());
    oled_application_task(app_mode);
    BuzzerPhrase400_Task(HAL_GetTick());
    if (((app_mode == APP_MODE_INTEGRATED || app_mode == APP_MODE_FIXED_BYPASS) ||
         app_mode == APP_MODE_LINE_ONLY) &&
        LineObstacleBypass_GetState() == LINE_BYPASS_IDLE)
    {
      WheelSpeedObserver_Task();
      approach_speed_cps = app_approach_speed_cps();
      emergency_distance_cm =
          app_emergency_distance_cm(approach_speed_cps);
    }

    if (app_mode == APP_MODE_STOPPED)
    {
      if (EXP7_VISION_ENABLED != 0U)
      {
        (void)vision_uart_take_event();
      }
      advanced_set_forward_speed_limit(MOTOR_PWM_PERIOD);
      advanced_stop();
      app_buzzer_safety_write(GPIO_PIN_RESET, 0U);
      HAL_GPIO_WritePin(led1_GPIO_Port, led1_Pin, GPIO_PIN_SET);
      HAL_GPIO_WritePin(led2_GPIO_Port, led2_Pin, GPIO_PIN_SET);
      HAL_Delay(1U);
      continue;
    }

    if (app_mode == APP_MODE_SIGN_LINE || app_mode == APP_MODE_SIGN_GYRO_TANGENT)
    {
      sign_line_task(app_mode);
      HAL_Delay(1U);
      continue;
    }

    if (app_mode == APP_MODE_LINE_ONLY)
    {
      LineTrackingAction line_action;

      /* 丢弃纯寻线期间收到的视觉事件，避免切回 KEY1 后执行旧命令。 */
      if (EXP7_VISION_ENABLED != 0U)
      {
        (void)vision_uart_take_event();
      }
      app_buzzer_safety_write(GPIO_PIN_RESET, 0U);
      advanced_set_forward_speed_limit(MOTOR_PWM_PERIOD);
      passive_ultrasonic_motion_task();
      line_action = line_tracking_follow_once(EXP7_LINE_SPEED,
                                             (int16_t)MOTOR_PWM_PERIOD);
      if (line_action != LINE_ACTION_FORWARD &&
          line_action != LINE_ACTION_CROSSING)
      {
        UltrasonicMotion_Reset();
      }
      motion_telemetry_task();
      HAL_Delay(1U);
      continue;
    }

    /* After returning to the line, the diagonal IR pair can still see the
       obstacle beside/behind the car.  Keep displaying the real RGB state,
       but suppress IR motor control until a delay plus consecutive clear
       samples prove that the old obstacle has been left behind. */
    if (bypass_rearm_pending != 0U)
    {
      if (!ir_status.left_obstacle && !ir_status.right_obstacle)
      {
        if (bypass_ir_clear_samples < BYPASS_REARM_CLEAR_SAMPLES)
        {
          ++bypass_ir_clear_samples;
        }
      }
      else
      {
        bypass_ir_clear_samples = 0U;
      }

      if (tick_reached(HAL_GetTick(), bypass_rearm_not_before_ms) &&
          bypass_ir_clear_samples >= BYPASS_REARM_CLEAR_SAMPLES)
      {
        bypass_rearm_pending = 0U;
      }
      else
      {
        /* RGB was already updated from the unmodified reading above. */
        ir_status.left_obstacle = false;
        ir_status.right_obstacle = false;
      }
    }

    /* KEY1 obstacle-on-line bypass owns all four motors until it either finds
       the line on the far side, faults, or the user changes mode/stops. */
    if (LineObstacleBypass_GetState() != LINE_BYPASS_IDLE)
    {
      LineTrackingReading bypass_line;
      LineObstacleBypassInput bypass_input;
      LineObstacleBypassState bypass_state;
      uint8_t bypass_front_obstacle;
      LineSensorSample sample;
      uint32_t through_ms = HAL_GetTick();

      while (LineSensorSample_PopThrough(&sample, through_ms))
        LineObstacleBypass_ObserveRawSensors(sample.mask, sample.time_ms);

      /* Only a shot started on this forward leg may interrupt it. Echoes
         collected while turning still face the original obstacle. */
      bypass_front_obstacle = LineBypassRange_Task(
          LineObstacleBypass_GetState() == LINE_BYPASS_DRIVING,
          EXP7_ULTRASONIC_BYPASS_STOP_CM);

      bypass_line = line_tracking_read();
      make_bypass_input(&bypass_input, &bypass_line, &ir_status);
      bypass_input.front_obstacle = bypass_front_obstacle;
      LineObstacleBypass_Task(&bypass_input);
      bypass_state = LineObstacleBypass_GetState();

      /* Cancel lower-priority audio before any optional diagnostic output can
         delay this active avoidance/safety owner. */
      if (bypass_state == LINE_BYPASS_FAULT)
      {
        uint8_t fault_code = encoder_fault_beep_code(
            LineObstacleBypass_GetFaultMask());
        uint32_t phase = HAL_GetTick() % 2500U;

        app_buzzer_safety_write(
            (phase < (uint32_t)fault_code * 250U &&
             (phase % 250U) < 100U) ? GPIO_PIN_SET : GPIO_PIN_RESET,
            1U);
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
        bypass_rearm_not_before_ms = HAL_GetTick() +
                                     BYPASS_REARM_DELAY_MS;
        UltrasonicAvoid_ResumeFollowing();
        (void)line_tracking_follow_once(EXP7_LINE_SPEED, ultrasonic_forward_speed_limit);
        WheelSpeedObserver_Start();
        last_fast_speed_cps = 0U;
        last_fast_speed_ms = HAL_GetTick();
      }
      else if (bypass_state == LINE_BYPASS_FAULT)
      {
        HAL_GPIO_WritePin(LRGB_R_GPIO_Port, LRGB_R_Pin, GPIO_PIN_SET);
      }
      bypass_telemetry_task();
      motion_telemetry_task();
      HAL_Delay(1U);
      continue;
    }

    /* A directional IR detection can start the detour before the front
       ultrasonic reaches its dynamic 20..32 cm emergency threshold. */
    {
      int8_t confirmed_direction;

      if (EXP7_IR_AVOID_ENABLED && confirmed_ir_bypass_direction(&ir_status,
                                        &confirmed_direction) != 0U)
      {
        line_tracking_reset();
        WheelSpeedObserver_Stop();
        LineBypassRange_Reset();
        (void)LineObstacleBypass_StartWithSpeed(confirmed_direction,
                                                approach_speed_cps);
        last_bypass_uart_ms = HAL_GetTick() - 200U;
        DiagnosticUart_WriteString("BYP2 START IR V=");
        DiagnosticUart_WriteUnsigned(approach_speed_cps);
        DiagnosticUart_WriteString(" E=");
        DiagnosticUart_WriteUnsigned(emergency_distance_cm);
        DiagnosticUart_WriteString("\r\n");
        app_buzzer_safety_write(GPIO_PIN_RESET, 1U);
        HAL_Delay(1U);
        continue;
      }
    }

    /* 中断采集和超时处理必须在主循环持续运行；回调的前进命令只给出
       安全速度上限，避免覆盖下方的循迹/视觉动作。 */
    advanced_set_forward_speed_limit(MOTOR_PWM_PERIOD);
    UltrasonicAvoid_SetEmergencyDistance(emergency_distance_cm);
    UltrasonicAvoid_Task();
    ultrasonic_state = UltrasonicAvoid_GetState();
    update_ultrasonic_buzzer(ultrasonic_state);
    show_ultrasonic_fault(ultrasonic_state);

    if (ultrasonic_state == ULTRASONIC_AVOID_STOPPING ||
        ultrasonic_state == ULTRASONIC_AVOID_BACKING ||
        ultrasonic_state == ULTRASONIC_AVOID_GUARD ||
        ultrasonic_state == ULTRASONIC_AVOID_TURNING)
    {
      line_tracking_reset();
      WheelSpeedObserver_Stop();
      LineBypassRange_Reset();
      (void)LineObstacleBypass_StartWithSpeed(
          choose_bypass_direction(&ir_status), approach_speed_cps);
      last_bypass_uart_ms = HAL_GetTick() - 200U;
      DiagnosticUart_WriteString("BYP2 START US V=");
      DiagnosticUart_WriteUnsigned(approach_speed_cps);
      DiagnosticUart_WriteString(" E=");
      DiagnosticUart_WriteUnsigned(emergency_distance_cm);
      DiagnosticUart_WriteString("\r\n");
      HAL_Delay(1U);
      continue;
    }

    if (ultrasonic_state != ULTRASONIC_AVOID_FORWARD)
    {
      /* 等待、停车、后退、换向保护、旋转和冷却阶段均由超声波
         状态机独占电机，普通循迹/视觉不能覆盖脱困动作。 */
      advanced_set_forward_speed_limit(MOTOR_PWM_PERIOD);
      motion_telemetry_task();
      HAL_Delay(1U);
      continue;
    }

    if (ultrasonic_forward_speed_limit <= 0)
    {
      /* 刚从 WAIT_SAFE 进入前进态时，等下一次有效回调发布速度上限。 */
      advanced_stop();
      motion_telemetry_task();
      HAL_Delay(1U);
      continue;
    }

    advanced_set_forward_speed_limit(ultrasonic_forward_speed_limit);

    experiment7_integrated_once();
    /* 在循迹/红外/视觉动作判定之后发送，转弯引起的无效状态不会
       被误报成一帧有效速度。 */
    motion_telemetry_task();

    HAL_Delay(1U);
  }
}

/* PG11 红外遥控与 PF12 超声波共用唯一 HAL EXTI 回调入口。 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  IrRemote_EXTI_Callback(GPIO_Pin);
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

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif
