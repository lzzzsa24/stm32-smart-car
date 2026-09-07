/*
 * 实验七：PF13/PF14/PF15/PG0 四路数字循迹
 *
 * 传感器输出低电平表示黑线。该文件是黑线位置外环，只生成左右目标
 * CPS；DriveBase 使用四路编码器形成各轮速度内环。丢线后四轮
 * 持续同向原地搜索并播放预制蜂鸣，见线确认后恢复。
 */

#include "line_tracking.h"
#include "line_recovery.h"
#include "line_sensor_sample.h"
#include "line_fault_log.h"

#include "drive_base.h"
#include "main.h"
#include "motorPWM.h"

static uint8_t no_line_forward_enabled = 1U;
static uint8_t line_has_been_seen;
static int8_t predicted_turn_direction;
static uint8_t direction_crossing_hold;
static uint8_t direction_hint_mask;
static int8_t ambiguous_inner_side;
static uint8_t ambiguous_inner_mask;
static uint32_t ambiguous_inner_last_seen_ms;
static int8_t direction_candidate;
static uint32_t direction_candidate_since_ms;
static uint32_t direction_last_seen_ms;
static uint8_t direction_center_active;
static uint32_t direction_center_since_ms;
static int8_t recovery_turn_direction;
static uint8_t smooth_mode_enabled;
static uint8_t smooth_filter_valid;
static int16_t smooth_error_q8;
static int16_t smooth_previous_error_q8;
static uint32_t smooth_last_update_ms;
static uint8_t smooth_centered_active;
static uint32_t smooth_centered_since_ms;
static uint32_t smooth_ramp_update_ms;
static int16_t smooth_straight_pwm;
static uint16_t smooth_turn_gain_percent = 100U;

typedef enum
{
  LINE_RECOVERY_NORMAL = 0U,
  LINE_RECOVERY_ACTIVE,
  LINE_RECOVERY_SETTLE,
  LINE_RECOVERY_STOPPED
} LineRecoveryState;

static LineRecoveryState recovery_state;
static uint32_t recovery_state_started_ms;
static uint8_t crossing_active, middle_recent_valid;
static uint32_t crossing_last_ms, middle_last_ms;
static int8_t corner_candidate;
static uint32_t corner_since_ms, corner_last_ms;
static uint32_t sample_overwritten;
static uint32_t last_observation_ms;
static uint8_t last_edge_mask, last_wide_mask;
static uint32_t last_edge_ms, last_wide_ms;
static int8_t last_logged_side;

#define TRACKING_HINT_CONFIRM_MS                4U
#define TRACKING_CROSS_CLEAR_MS               100U
#define TRACKING_NARROW_GAP_MS                 60U
#define TRACKING_CORNER_CONFIRM_MS             12U
#define TRACKING_HINT_MAX_AGE_MS              200U
/* Allow one short broad bend plus the 100-ms crossing tail to obscure a
   recent side. This is an absolute age from real directional evidence;
   repeated wide samples never renew it. */
#define TRACKING_CROSS_HINT_MAX_AGE_MS        400U
#define TRACKING_INNER_PROBE_MAX_AGE_MS       200U
#define TRACKING_HINT_CENTER_CLEAR_MS          80U
#define TRACKING_REACQUIRE_SETTLE_MS         500U
#define TRACKING_MIN_INNER_PWM             2200
#define TRACKING_MIN_OUTER_PWM             3000
#define TRACKING_SETTLE_INNER_PWM           2200
#define TRACKING_SETTLE_OUTER_PWM           2400
#define TRACKING_SETTLE_CENTER_PWM          2200
#define TRACKING_NORMAL_CENTER_PWM          2700
#define TRACKING_SMOOTH_UPDATE_MS              10U
#define TRACKING_SMOOTH_STEER_LIMIT          1400
#define TRACKING_SMOOTH_STEER_DEADBAND        100
#define TRACKING_SMOOTH_CURVE_CENTER_PWM      2800
#define TRACKING_SMOOTH_CURVE_SLOWDOWN_PWM     100
#define TRACKING_SMOOTH_STRAIGHT_BASE_PWM      2600
#define TRACKING_SMOOTH_STRAIGHT_MAX_PWM       2700
#define TRACKING_SMOOTH_CENTER_HOLD_MS          350U
#define TRACKING_SMOOTH_RAMP_INTERVAL_MS         20U
#define TRACKING_SMOOTH_RAMP_STEP_PWM             20

static int16_t clamp_speed(int32_t speed)
{
  if (speed <= 0)
  {
    return 0;
  }

  if (speed >= (int32_t)MOTOR_PWM_PERIOD)
  {
    return (int16_t)MOTOR_PWM_PERIOD;
  }

  return (int16_t)speed;
}

static int16_t scale_speed(int16_t speed, uint16_t percent)
{
  return clamp_speed(((int32_t)speed * percent) / 100);
}

static int16_t ensure_minimum_speed(int16_t speed, int16_t minimum)
{
  if (speed < minimum)
  {
    return clamp_speed(minimum);
  }

  return speed;
}

static int16_t turn_speed_for_gain(int16_t normal_speed)
{
  int32_t extra_percent;
  int32_t speed;

  if (smooth_turn_gain_percent <= 100U)
  {
    return normal_speed;
  }
  extra_percent = (int32_t)smooth_turn_gain_percent - 100L;
  speed = (int32_t)normal_speed +
      (((int32_t)MOTOR_PWM_PERIOD - normal_speed) * extra_percent) / 100L;
  return clamp_speed(speed);
}

static void command_set_pwm(LineTrackingCommand *command,
                            int16_t left_pwm,
                            int16_t right_pwm,
                            LineTrackingAction action)
{
  if (command == 0) return;
  command->left_cps = DriveBase_EquivalentCpsFromPwm(left_pwm);
  command->right_cps = DriveBase_EquivalentCpsFromPwm(right_pwm);
  /* Preparing does not own the motors. DriveBase accepts only exact targets;
     a consumer applying a speed cap must rebind the claim to the final pair. */
  DriveBase_PrepareLineTurnAssist(command->left_cps, command->right_cps);
  command->action = action;
  command->valid = 1U;
}

static void command_stop(LineTrackingCommand *command)
{
  command_set_pwm(command, 0, 0, LINE_ACTION_STOP);
}
void line_tracking_apply_command(const LineTrackingCommand *command, int16_t forward_limit_pwm)
{
  int32_t left, right, maximum, limit;
  if (!command || !command->valid) return;
  left = command->left_cps; right = command->right_cps;
  if (left >= 0L && right >= 0L)
  {
    limit = DriveBase_EquivalentCpsFromPwm(clamp_speed(forward_limit_pwm));
    maximum = left > right ? left : right;
    if (maximum > limit && maximum > 0L)
    {
      left = (int32_t)(((int64_t)left * limit) / maximum);
      right = (int32_t)(((int64_t)right * limit) / maximum);
    }
  }
  /* KEY1's ultrasonic cap changes the targets. A claim prepared before the
     cap is deliberately rejected by DriveBase, so bind only the final pair. */
  DriveBase_PrepareLineTurnAssist(left, right);
  if (left == 0L && right == 0L) DriveBase_Stop(DRIVE_STOP_COAST);
  else DriveBase_SetSideCps(left, right);
}

static void recovery_stop(LineRecoveryStopReason reason)
{
  LineRecovery_Stop(reason);
  recovery_state = LINE_RECOVERY_STOPPED;
}

static uint8_t read_black(GPIO_TypeDef *port, uint16_t pin)
{
  return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET ? 1U : 0U;
}

int8_t line_tracking_direction_evidence(const LineTrackingReading *r)
{
  if (!r) return 0;
  /* Include a contiguous three-probe overlap, without treating two separated
     black islands or both outer probes as a side. This never grants a spin. */
  if (r->x2_black && !r->x4_black && (!r->x3_black || r->x1_black)) return -1;
  if (r->x4_black && !r->x2_black && (!r->x1_black || r->x3_black)) return 1;
  return 0;
}

static uint8_t reading_mask(const LineTrackingReading *r)
{
  return (uint8_t)(r->x1_black | (r->x2_black << 1) |
      (r->x3_black << 2) | (r->x4_black << 3));
}

/* Observe sensor position, never the filtered motor correction. A newly
   opposing observation invalidates the old hint while it is being confirmed. */
static void update_direction_hint(const LineTrackingReading *reading,
                                  uint8_t active_count, uint32_t now)
{
  int16_t position = -3 * reading->x2_black - reading->x1_black +
                       reading->x3_black + 3 * reading->x4_black;
  int8_t side = position < 0 ? -1 : (position > 0 ? 1 : 0);

  /* One side-bearing outer observation (including an adjacent triple) is
     enough for memory. Broad-pattern forward priority and the narrow corner
     command gate are separate, so this observation cannot itself start a spin. */
  if (line_tracking_direction_evidence(reading))
  {
    ambiguous_inner_side = 0;
    ambiguous_inner_mask = 0U;
    predicted_turn_direction = direction_candidate = side;
    direction_crossing_hold = 0U;
    direction_last_seen_ms = direction_candidate_since_ms = now;
    direction_hint_mask = reading_mask(reading);
    direction_center_active = 0U;
    return;
  }
  if (reading->x2_black && reading->x4_black)
  {
    ambiguous_inner_side = 0;
    ambiguous_inner_mask = 0U;
    predicted_turn_direction = 0;
    direction_crossing_hold = 0U;
    direction_candidate = 0;
    direction_center_active = 0U;
    return;
  }
  if (active_count == 0U)
  {
    direction_candidate = 0;
    direction_center_active = 0U;
    return;
  }
  /* A single inner probe says where the line intersects this sensor row, not
     which way an oblique track continues. Keep proportional steering, but do
     not turn this geometrically ambiguous snapshot into a persistent hint. */
  if (active_count == 1U && (reading->x1_black || reading->x3_black))
  {
    ambiguous_inner_side = side;
    ambiguous_inner_mask = reading_mask(reading);
    ambiguous_inner_last_seen_ms = now;
    direction_candidate = 0;
    direction_center_active = 0U;
    return;
  }
  if (side == 0)
  {
    ambiguous_inner_side = 0;
    ambiguous_inner_mask = 0U;
    direction_candidate = 0;
    if (direction_center_active == 0U)
    {
      direction_center_active = 1U;
      direction_center_since_ms = now;
    }
    if (now - direction_center_since_ms >= TRACKING_HINT_CENTER_CLEAR_MS)
    {
      predicted_turn_direction = 0;
      direction_crossing_hold = 0U;
    }
    return;
  }
  direction_center_active = 0U;
  ambiguous_inner_side = 0;
  ambiguous_inner_mask = 0U;
  if (side != direction_candidate)
  {
    direction_candidate = side;
    direction_candidate_since_ms = now;
    if (predicted_turn_direction != side)
    { predicted_turn_direction = 0; direction_crossing_hold = 0U; }
  }
  if (now - direction_candidate_since_ms >= TRACKING_HINT_CONFIRM_MS)
  {
    predicted_turn_direction = side;
    direction_crossing_hold = 0U;
    direction_last_seen_ms = now;
    direction_hint_mask = reading_mask(reading);
  }
}

void line_tracking_init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;

  gpio.Pin = TRACK_X1_Pin | TRACK_X2_Pin | TRACK_X3_Pin;
  HAL_GPIO_Init(TRACK_X1_GPIO_Port, &gpio);

  gpio.Pin = TRACK_X4_Pin;
  HAL_GPIO_Init(TRACK_X4_GPIO_Port, &gpio);

  line_tracking_reset();
  LineSensorSample_Start();
}

void line_tracking_reset(void)
{
  LineSensorSample_Reset();
  sample_overwritten = 0U;
  last_edge_mask = last_wide_mask = 0U;
  last_logged_side = 0;
  last_observation_ms = HAL_GetTick();
  DriveBase_SetLineFaultObservation(0U, 0U, 0U);
  LineRecovery_Reset();
  crossing_active = middle_recent_valid = 0U;
  corner_candidate = 0;
  crossing_last_ms = middle_last_ms = corner_since_ms = corner_last_ms = HAL_GetTick();
  line_has_been_seen = 0U;
  predicted_turn_direction = 0;
  direction_crossing_hold = 0U;
  direction_hint_mask = 0U;
  ambiguous_inner_side = 0;
  ambiguous_inner_mask = 0U;
  ambiguous_inner_last_seen_ms = HAL_GetTick();
  direction_candidate = 0;
  direction_center_active = 0U;
  direction_candidate_since_ms = HAL_GetTick();
  direction_last_seen_ms = HAL_GetTick();
  direction_center_since_ms = HAL_GetTick();
  recovery_turn_direction = 0;
  smooth_filter_valid = 0U;
  smooth_error_q8 = 0;
  smooth_previous_error_q8 = 0;
  smooth_last_update_ms = HAL_GetTick();
  smooth_centered_active = 0U;
  smooth_centered_since_ms = HAL_GetTick();
  smooth_ramp_update_ms = HAL_GetTick();
  smooth_straight_pwm = TRACKING_SMOOTH_STRAIGHT_BASE_PWM;
  recovery_state = LINE_RECOVERY_NORMAL;
  recovery_state_started_ms = HAL_GetTick();
}

void line_tracking_set_no_line_forward(uint8_t enable)
{
  no_line_forward_enabled = enable != 0U ? 1U : 0U;
}

void line_tracking_set_smooth_mode(uint8_t enable)
{
  smooth_mode_enabled = enable != 0U ? 1U : 0U;
  smooth_filter_valid = 0U;
  smooth_error_q8 = 0;
  smooth_previous_error_q8 = 0;
  smooth_last_update_ms = HAL_GetTick();
  smooth_centered_active = 0U;
  smooth_centered_since_ms = HAL_GetTick();
  smooth_ramp_update_ms = HAL_GetTick();
  smooth_straight_pwm = TRACKING_SMOOTH_STRAIGHT_BASE_PWM;
}

void line_tracking_set_turn_gain_percent(uint16_t percent)
{
  if (percent < 50U)
  {
    percent = 50U;
  }
  else if (percent > 200U)
  {
    percent = 200U;
  }
  smooth_turn_gain_percent = percent;
}

LineTrackingReading line_tracking_read(void)
{
  LineTrackingReading reading = {0};
  uint32_t irq = __get_PRIMASK();

  /* Keep SysTick from inserting a newer observation between the timestamp
     and the four GPIO reads. Restore the caller's interrupt state. */
  __disable_irq();
  reading.sampled_ms = HAL_GetTick();
  reading.x1_black = read_black(TRACK_X1_GPIO_Port, TRACK_X1_Pin);
  reading.x2_black = read_black(TRACK_X2_GPIO_Port, TRACK_X2_Pin);
  reading.x3_black = read_black(TRACK_X3_GPIO_Port, TRACK_X3_Pin);
  reading.x4_black = read_black(TRACK_X4_GPIO_Port, TRACK_X4_Pin);
  reading.sampled_time_valid = 1U;
  __set_PRIMASK(irq);
  return reading;
}

static uint8_t transverse(const LineTrackingReading *r)
{
  uint8_t n = (uint8_t)(r->x1_black + r->x2_black + r->x3_black + r->x4_black);
  return n >= 3U || (r->x2_black && r->x4_black) ||
      (r->x2_black && r->x3_black) || (r->x1_black && r->x4_black);
}
static uint8_t unambiguous_edge(const LineTrackingReading *r)
{
  return (r->x2_black && !r->x3_black && !r->x4_black) ||
      (r->x4_black && !r->x1_black && !r->x2_black);
}
static void observe_raw_position(const LineTrackingReading *r, uint32_t now)
{
  uint8_t mask = reading_mask(r);
  if (unambiguous_edge(r)) { last_edge_mask = mask; last_edge_ms = now; }
  if (transverse(r)) { last_wide_mask = mask; last_wide_ms = now; }
}
static void record_search(uint32_t now, LineSearchSource source)
{
  LineSearchRecord r = {0};
  r.time_ms = now;
  r.edge_mask = last_edge_mask; r.wide_mask = last_wide_mask;
  r.edge_age_ms = last_edge_mask ? now - last_edge_ms : UINT32_MAX;
  r.wide_age_ms = last_wide_mask ? now - last_wide_ms : UINT32_MAX;
  r.queue_overwritten = LineSensorSample_Overwritten();
  if (source == LINE_SEARCH_INNER_PROBE)
  {
    r.hint = ambiguous_inner_side;
    r.hint_mask = ambiguous_inner_mask;
    r.hint_age_ms = now - ambiguous_inner_last_seen_ms;
  }
  else
  {
    r.hint = predicted_turn_direction;
    r.hint_mask = predicted_turn_direction ? direction_hint_mask : 0U;
    r.hint_age_ms = predicted_turn_direction ? now - direction_last_seen_ms : UINT32_MAX;
  }
  r.chosen_side = recovery_turn_direction > 0 ? 1 : -1;
  last_logged_side = r.chosen_side;
  r.source = source;
  LineFaultLog_RecordSearch(&r);
}
static void observe_crossing(uint32_t now)
{
  DriveBaseTelemetry telemetry;
  LineRecovery_Commit();
  DriveBase_GetTelemetry(&telemetry);
  if (telemetry.mode == DRIVE_BASE_BRAKING) DriveBase_Stop(DRIVE_STOP_COAST);
  recovery_state = LINE_RECOVERY_NORMAL;
  crossing_active = 1U;
  crossing_last_ms = now;
  middle_recent_valid = 0U;
  ambiguous_inner_side = 0;
  ambiguous_inner_mask = 0U;
  /* A broad mark cancels the motor turn, but is not evidence for the opposite
     side. Keep only a still-recent hint; never extend its acquisition time. */
  if (predicted_turn_direction && now - direction_last_seen_ms <=
      (direction_crossing_hold ? TRACKING_CROSS_HINT_MAX_AGE_MS : TRACKING_HINT_MAX_AGE_MS))
    direction_crossing_hold = 1U;
  else
  { predicted_turn_direction = 0; direction_crossing_hold = 0U; }
  corner_candidate = recovery_turn_direction = direction_candidate = 0;
  direction_center_active = 0U;
  smooth_filter_valid = smooth_centered_active = 0U;
  smooth_straight_pwm = TRACKING_SMOOTH_STRAIGHT_BASE_PWM;
}
static void consume_sampled_evidence(uint32_t through_ms)
{
  LineSensorSample sample;
  unsigned budget = LINE_SENSOR_QUEUE_SIZE;
  uint32_t lost = LineSensorSample_Overwritten();
  if (lost != sample_overwritten)
  {
    /* Missing history cannot support an old normal-mode directional hint. */
    predicted_turn_direction = direction_candidate = corner_candidate = 0;
    direction_crossing_hold = 0U;
    ambiguous_inner_side = 0;
    ambiguous_inner_mask = 0U;
    direction_center_active = 0U;
    sample_overwritten = lost;
  }
  while (budget-- && LineSensorSample_PopThrough(&sample, through_ms))
  {
    LineTrackingReading r = {0};
    uint8_t n;
    if ((int32_t)(sample.time_ms - last_observation_ms) <= 0) continue;
    if (through_ms - sample.time_ms > TRACKING_HINT_MAX_AGE_MS) continue;
    r.x1_black = sample.mask & 1U;
    r.x2_black = (sample.mask >> 1) & 1U;
    r.x3_black = (sample.mask >> 2) & 1U;
    r.x4_black = (sample.mask >> 3) & 1U;
    n = (uint8_t)(r.x1_black + r.x2_black + r.x3_black + r.x4_black);
    observe_raw_position(&r, sample.time_ms);
    if (n) line_has_been_seen = 1U;
    if (transverse(&r))
    {
      if (line_tracking_direction_evidence(&r)) update_direction_hint(&r, n, sample.time_ms);
      observe_crossing(sample.time_ms);
      continue;
    }
    /* A main-loop corner timer cannot span a contradictory ISR observation. */
    if (line_tracking_direction_evidence(&r) != corner_candidate) corner_candidate = 0;
    if ((r.x1_black || r.x3_black) && !r.x2_black && !r.x4_black)
    { middle_recent_valid = 1U; middle_last_ms = sample.time_ms; }
    /* Observe all narrow evidence even during the crossing motor guard or
       active recovery, so centre/opposite-side evidence can expire a hold. */
    update_direction_hint(&r, n, sample.time_ms);
    if (crossing_active && sample.time_ms - crossing_last_ms < TRACKING_CROSS_CLEAR_MS)
    {
      continue;
    }
    if (recovery_state == LINE_RECOVERY_ACTIVE)
      LineRecovery_ObserveDirection(&r, sample.time_ms);
  }
}
LineTrackingAction line_tracking_compute(const LineTrackingReading *reading,
                                         int16_t base_speed,
                                         LineTrackingCommand *command)
{
  int16_t turn_inner_speed;
  int16_t turn_outer_speed;
  int16_t center_speed;
  int16_t weighted_sum;
  int16_t line_position;
  uint8_t active_count;
  uint8_t center_visible;
  uint8_t middle_only;
  int8_t edge_side;
  uint8_t settling = 0U;
  uint8_t inner_probe_start = 0U;
  LineSearchSource search_source = LINE_SEARCH_DEFAULT;
  uint32_t now = HAL_GetTick();

  if (command == 0 || reading == 0)
  {
    return LINE_ACTION_STOP;
  }
  if (reading->sampled_time_valid) now = reading->sampled_ms;
  command_stop(command);

  if (base_speed <= 0)
  {
    line_tracking_reset();
    command_stop(command);
    return LINE_ACTION_STOP;
  }

  DriveBase_SetLineFaultObservation(1U,
      (uint8_t)((reading->x1_black ? 1U : 0U) | (reading->x2_black ? 2U : 0U) |
                (reading->x3_black ? 4U : 0U) | (reading->x4_black ? 8U : 0U)),
      (uint8_t)recovery_state);
  active_count = (uint8_t)(reading->x1_black + reading->x2_black +
                           reading->x3_black + reading->x4_black);
  center_visible = (reading->x1_black || reading->x3_black) ? 1U : 0U;
  middle_only = center_visible && !reading->x2_black && !reading->x4_black;
  if (active_count != 0U)
  {
    line_has_been_seen = 1U;
  }

  if (DriveBase_GetFaultMask() != 0U)
  {
    recovery_stop(LINE_REC_STOP_DRIVE_FAULT);
    return LINE_ACTION_STOP;
  }
  if (recovery_state == LINE_RECOVERY_STOPPED) return LINE_ACTION_STOP;
  /* Drain only history preceding this GPIO snapshot, not compute time.
     Otherwise an ISR edge between read() and compute() is replayed first,
     then erased by the older live reading. Ticks after this boundary remain
     queued, including those arriving after the last pop releases IRQs. */
  consume_sampled_evidence(now);
  last_observation_ms = now;
  observe_raw_position(reading, now);
  /* Wide or non-adjacent black detections override a previously latched turn.
     In particular X2+X1+X3 (only rightmost white) must never keep spinning. */
  if (transverse(reading))
  {
    if (line_tracking_direction_evidence(reading)) update_direction_hint(reading, active_count, now);
    observe_crossing(now);
    command_set_pwm(command, TRACKING_SETTLE_CENTER_PWM, TRACKING_SETTLE_CENTER_PWM, LINE_ACTION_CROSSING);
    return command->action;
  }
  if (middle_only) { middle_recent_valid = 1U; middle_last_ms = now; }
  update_direction_hint(reading, active_count, now);
  if (crossing_active)
  {
    if (now - crossing_last_ms < TRACKING_CROSS_CLEAR_MS)
    {
      command_set_pwm(command, TRACKING_SETTLE_CENTER_PWM, TRACKING_SETTLE_CENTER_PWM, LINE_ACTION_CROSSING);
      return command->action;
    }
    crossing_active = 0U;
  }
  edge_side = reading->x2_black && !reading->x3_black && !reading->x4_black ? -1 :
              (reading->x4_black && !reading->x1_black && !reading->x2_black ? 1 : 0);
  if ((recovery_state == LINE_RECOVERY_NORMAL || recovery_state == LINE_RECOVERY_SETTLE) && edge_side)
  {
    if (corner_candidate != edge_side || now - corner_last_ms > 30U)
    { corner_candidate = edge_side; corner_since_ms = now; }
    corner_last_ms = now;
    if (now - corner_since_ms >= TRACKING_CORNER_CONFIRM_MS)
    {
      recovery_turn_direction = edge_side;
      record_search(now, LINE_SEARCH_CORNER);
      smooth_filter_valid = smooth_centered_active = 0U;
      smooth_straight_pwm = TRACKING_SMOOTH_STRAIGHT_BASE_PWM;
      LineRecovery_BeginCorner(recovery_turn_direction, now);
      recovery_state = LINE_RECOVERY_ACTIVE;
    }
    else
    {
      command_set_pwm(command, edge_side < 0 ? TRACKING_SETTLE_INNER_PWM : TRACKING_SETTLE_OUTER_PWM,
                      edge_side < 0 ? TRACKING_SETTLE_OUTER_PWM : TRACKING_SETTLE_INNER_PWM,
                      edge_side < 0 ? LINE_ACTION_LEFT_ADJUST : LINE_ACTION_RIGHT_ADJUST);
      return command->action;
    }
  }
  else corner_candidate = 0;
  if (recovery_state == LINE_RECOVERY_ACTIVE)
  {
    LineRecoveryResult result = LineRecovery_Step(reading, command, now);
    if (result != LINE_RECOVERY_FAILED && LineRecovery_GetDirection() != last_logged_side)
    {
      recovery_turn_direction = LineRecovery_GetDirection();
      record_search(now, LINE_SEARCH_CORRECTION);
    }
    if (result == LINE_RECOVERY_CAPTURED)
    {
      /* A one-sided inner capture is geometrically ambiguous: it proves line
         contact, not the direction of an oblique track. Re-loss during settle
         therefore reuses the direction that actually found this contact. */
      recovery_turn_direction = LineRecovery_GetDirection();
      predicted_turn_direction = direction_candidate = 0;
      direction_last_seen_ms = direction_candidate_since_ms = now;
      direction_hint_mask = 0U;
      direction_crossing_hold = 0U;
      ambiguous_inner_side = 0;
      ambiguous_inner_mask = 0U;
      direction_center_active = 0U;
      recovery_state = LINE_RECOVERY_SETTLE;
      recovery_state_started_ms = now;
    }
    else if (result == LINE_RECOVERY_FAILED)
    {
      recovery_stop(LineRecovery_GetStopReason());
      command_stop(command);
    }
    if (result != LINE_RECOVERY_CAPTURED) return command->action;
  }
  if (recovery_state == LINE_RECOVERY_STOPPED)
  {
    command_stop(command);
    return LINE_ACTION_STOP;
  }
  if (active_count == 0U)
  {
    smooth_filter_valid = 0U;
    smooth_centered_active = 0U;
    smooth_straight_pwm = TRACKING_SMOOTH_STRAIGHT_BASE_PWM;
    if (middle_recent_valid && now - middle_last_ms <= TRACKING_NARROW_GAP_MS)
    {
      command_set_pwm(command, TRACKING_SETTLE_CENTER_PWM, TRACKING_SETTLE_CENTER_PWM, LINE_ACTION_FORWARD);
      return command->action;
    }
    if (no_line_forward_enabled != 0U && line_has_been_seen == 0U)
    {
      command_set_pwm(command, base_speed, base_speed, LINE_ACTION_FORWARD);
      return LINE_ACTION_FORWARD;
    }
    if (predicted_turn_direction != 0 && now - direction_last_seen_ms <=
        (direction_crossing_hold ? TRACKING_CROSS_HINT_MAX_AGE_MS : TRACKING_HINT_MAX_AGE_MS))
    {
      recovery_turn_direction = predicted_turn_direction;
      search_source = direction_crossing_hold ? LINE_SEARCH_CROSS_HINT : LINE_SEARCH_HINT;
    }
    else if (recovery_state == LINE_RECOVERY_NORMAL &&
             ambiguous_inner_side != 0 &&
             now - ambiguous_inner_last_seen_ms <= TRACKING_INNER_PROBE_MAX_AGE_MS)
    {
      recovery_turn_direction = ambiguous_inner_side;
      search_source = LINE_SEARCH_INNER_PROBE;
      inner_probe_start = 1U;
    }
    else if (recovery_state == LINE_RECOVERY_NORMAL) recovery_turn_direction = 0;
    else search_source = LINE_SEARCH_REJOIN;
    record_search(now, search_source);
    if (inner_probe_start)
      LineRecovery_BeginAmbiguous(recovery_turn_direction, now);
    else
      LineRecovery_Begin(recovery_turn_direction, now);
    ambiguous_inner_side = 0;
    ambiguous_inner_mask = 0U;
    recovery_state = LINE_RECOVERY_ACTIVE;
    /* Issue the new spin targets in this same iteration. Repeated narrow-line
       captures/losses must not insert a brake or a generic zero-speed command. */
    if (LineRecovery_Step(reading, command, now) == LINE_RECOVERY_FAILED)
    {
      recovery_stop(LineRecovery_GetStopReason());
      command_stop(command);
    }
    return command->action;
  }
  if (recovery_state == LINE_RECOVERY_SETTLE)
  {
    if (now - recovery_state_started_ms >= TRACKING_REACQUIRE_SETTLE_MS &&
        middle_recent_valid && now - middle_last_ms <= TRACKING_NARROW_GAP_MS)
    {
      LineRecovery_Commit();
      recovery_state = LINE_RECOVERY_NORMAL;
      /* End only the old recovery fallback. The position observer may just
         have confirmed the next corner (including queued samples); changing
         speed/state must not erase that fresh hint or its confirmation. */
      recovery_turn_direction = 0;
    }
    else settling = 1U;
  }
  if (settling)
  {
    /* Single-side outer evidence already returned to continuous turning.
       Middle and ambiguous/crossing patterns receive low-speed guidance. */
    weighted_sum = (int16_t)(-3 * reading->x2_black - reading->x1_black +
                             reading->x3_black + 3 * reading->x4_black);
    if (reading->x2_black && reading->x4_black)
      command_set_pwm(command, TRACKING_SETTLE_CENTER_PWM, TRACKING_SETTLE_CENTER_PWM,
                      LINE_ACTION_CROSSING);
    else if (weighted_sum < 0)
      command_set_pwm(command, TRACKING_SETTLE_INNER_PWM, TRACKING_SETTLE_OUTER_PWM,
                      LINE_ACTION_LEFT_ADJUST);
    else if (weighted_sum > 0)
      command_set_pwm(command, TRACKING_SETTLE_OUTER_PWM, TRACKING_SETTLE_INNER_PWM,
                      LINE_ACTION_RIGHT_ADJUST);
    else
      command_set_pwm(command, TRACKING_SETTLE_CENTER_PWM, TRACKING_SETTLE_CENTER_PWM,
                      LINE_ACTION_FORWARD);
    return command->action;
  }

  turn_inner_speed = ensure_minimum_speed(
      scale_speed(base_speed, 55U), TRACKING_MIN_INNER_PWM);
  turn_outer_speed = ensure_minimum_speed(
      scale_speed(base_speed, 90U), TRACKING_MIN_OUTER_PWM);
  turn_outer_speed = turn_speed_for_gain(turn_outer_speed);
  center_speed = base_speed > TRACKING_NORMAL_CENTER_PWM
               ? TRACKING_NORMAL_CENTER_PWM : base_speed;

  if (active_count != 0U)
  {
    /* 物理从左到右按 X2、X1、X3、X4 排列。用位置加权处理组合状态，
       避免多个探头同时压线时在离散规则之间突然跳变。 */
    weighted_sum = (int16_t)(-3 * reading->x2_black - reading->x1_black +
                              reading->x3_black + 3 * reading->x4_black);
    line_position = weighted_sum / active_count;

    if (smooth_mode_enabled != 0U && settling == 0U &&
        line_position > -2 && line_position < 2)
    {
      int16_t raw_error_q8 = (int16_t)(((int32_t)weighted_sum * 256) /
                                       active_count);
      int16_t derivative_q8;
      int16_t steering;
      int32_t steering_work;
      int16_t magnitude;
      int16_t curve_center;
      int16_t left_target;
      int16_t right_target;
      uint8_t stable_center = (line_position == 0 &&
                               reading->x2_black == 0U &&
                               reading->x4_black == 0U) ? 1U : 0U;

      if (stable_center != 0U)
      {
        if (smooth_centered_active == 0U)
        {
          smooth_centered_active = 1U;
          smooth_centered_since_ms = now;
          smooth_ramp_update_ms = now;
          smooth_straight_pwm = TRACKING_SMOOTH_STRAIGHT_BASE_PWM;
        }
        else if (now - smooth_centered_since_ms >=
                 TRACKING_SMOOTH_CENTER_HOLD_MS &&
                 now - smooth_ramp_update_ms >=
                 TRACKING_SMOOTH_RAMP_INTERVAL_MS)
        {
          smooth_ramp_update_ms = now;
          if (smooth_straight_pwm < TRACKING_SMOOTH_STRAIGHT_MAX_PWM)
          {
            smooth_straight_pwm = clamp_speed(
                (int32_t)smooth_straight_pwm +
                TRACKING_SMOOTH_RAMP_STEP_PWM);
          }
        }
      }
      else
      {
        /* Raw sensor departure wins immediately over the filtered error so
           the car never carries straight-line boost into a bend. */
        smooth_centered_active = 0U;
        smooth_straight_pwm = TRACKING_SMOOTH_STRAIGHT_BASE_PWM;
      }

      if (smooth_filter_valid == 0U)
      {
        smooth_error_q8 = raw_error_q8;
        smooth_previous_error_q8 = raw_error_q8;
        smooth_last_update_ms = now;
        smooth_filter_valid = 1U;
      }
      else if (now - smooth_last_update_ms >= TRACKING_SMOOTH_UPDATE_MS)
      {
        smooth_previous_error_q8 = smooth_error_q8;
        /* 1/4 new sample, 3/4 history: suppress edge chatter without adding
           a long delay at the 10 ms control update rate. */
        smooth_error_q8 = (int16_t)(((int32_t)smooth_error_q8 * 3 +
                                    raw_error_q8) / 4);
        smooth_last_update_ms = now;
      }

      derivative_q8 = (int16_t)(smooth_error_q8 -
                                smooth_previous_error_q8);
      /* The derivative term previously amplified X1/X3 one-frame chatter and
         made a centred car alternate left/right.  Keep enough derivative for
         a real bend, but require a wider centre deadband. */
      steering_work = ((int32_t)smooth_error_q8 * 3) / 2 +
                      derivative_q8 / 2;
      steering_work = (steering_work * smooth_turn_gain_percent) / 100L;
      if (steering_work > TRACKING_SMOOTH_STEER_LIMIT)
      {
        steering = TRACKING_SMOOTH_STEER_LIMIT;
      }
      else if (steering_work < -TRACKING_SMOOTH_STEER_LIMIT)
      {
        steering = -TRACKING_SMOOTH_STEER_LIMIT;
      }
      else
      {
        steering = (int16_t)steering_work;
      }

      magnitude = smooth_error_q8 < 0
                ? (int16_t)-smooth_error_q8 : smooth_error_q8;
      curve_center = TRACKING_SMOOTH_CURVE_CENTER_PWM -
          (int16_t)(((int32_t)magnitude *
                     TRACKING_SMOOTH_CURVE_SLOWDOWN_PWM) / 256);
      left_target = clamp_speed((int32_t)curve_center + steering);
      right_target = clamp_speed((int32_t)curve_center - steering);
      left_target = ensure_minimum_speed(left_target,
                                         TRACKING_MIN_INNER_PWM);
      right_target = ensure_minimum_speed(right_target,
                                          TRACKING_MIN_INNER_PWM);

      if (steering < -TRACKING_SMOOTH_STEER_DEADBAND)
      {
        command_set_pwm(command, left_target, right_target,
                        LINE_ACTION_LEFT_ADJUST);
        return LINE_ACTION_LEFT_ADJUST;
      }
      if (steering > TRACKING_SMOOTH_STEER_DEADBAND)
      {
        command_set_pwm(command, left_target, right_target,
                        LINE_ACTION_RIGHT_ADJUST);
        return LINE_ACTION_RIGHT_ADJUST;
      }

      /* Only a continuously centred run earns the reduced 2600..2700
         straight boost. Curve/search targets retain their existing effort. */
      command_set_pwm(command, smooth_straight_pwm, smooth_straight_pwm,
                      LINE_ACTION_FORWARD);
      return LINE_ACTION_FORWARD;
    }

    if (line_position < 0)
    {
      command_set_pwm(command, turn_inner_speed, turn_outer_speed,
                      LINE_ACTION_LEFT_ADJUST);
      return LINE_ACTION_LEFT_ADJUST;
    }

    if (line_position > 0)
    {
      command_set_pwm(command, turn_outer_speed, turn_inner_speed,
                      LINE_ACTION_RIGHT_ADJUST);
      return LINE_ACTION_RIGHT_ADJUST;
    }
    command_set_pwm(command, center_speed, center_speed,
                    LINE_ACTION_FORWARD);
    return LINE_ACTION_FORWARD;
  }

  /* active_count==0 已在函数前半段处理，此处只作防御。 */
  command_stop(command);
  return LINE_ACTION_STOP;
}
