#include "line_bypass_turn.h"
#include "mpu6050_yaw.h"
#if MPU6050_BYPASS_ENABLED
#include "gyro_turn.h"
uint8_t LineBypassTurn_Start(int32_t angle_mdeg, int32_t cps)
{ return GyroTurn_Start(angle_mdeg, cps); }
void LineBypassTurn_Task(void) { GyroTurn_Task(); }
uint8_t LineBypassTurn_RequestStop(void) { return GyroTurn_RequestStop(); }
void LineBypassTurn_Stop(void) { GyroTurn_Stop(); }
LineBypassTurnState LineBypassTurn_GetState(void)
{ return (LineBypassTurnState)GyroTurn_GetState(); }
uint8_t LineBypassTurn_GetFaultMask(void)
{ return GyroTurn_GetFault() ? 0x10U : 0U; }
int32_t LineBypassTurn_GetAchievedAngleMdeg(void)
{ return GyroTurn_GetAchievedAngleMdeg(); }
#else
#include "drive_base.h"
#include "line_search_model.h"
#include "main.h"
#include "wheel_encoder.h"

#define BYPASS_TURN_SETTLE_MS 120U
#define BYPASS_TURN_SETTLE_LIMIT_MS 300U
#define BYPASS_TURN_CONTROLLER_FAULT 0x10U

static LineBypassTurnState state;
static WheelEncoderCounts start_counts;
static int32_t target_counts, turn_cps, turn_sign, achieved_mdeg;
static uint32_t started_ms, timeout_ms, stopped_ms;
static uint8_t settling, fault_mask;

static int64_t travel_sum(int32_t *minimum)
{
  WheelEncoderCounts current;
  int32_t travel[4];
  int64_t sum = 0;
  unsigned i;
  WheelEncoder_GetCounts(&current);
  /* Unsigned subtraction also handles a signed encoder-counter wrap. */
  travel[0] = (int32_t)((uint32_t)current.motor1 - (uint32_t)start_counts.motor1) * -turn_sign;
  travel[1] = (int32_t)((uint32_t)current.motor2 - (uint32_t)start_counts.motor2) * -turn_sign;
  travel[2] = (int32_t)((uint32_t)current.motor3 - (uint32_t)start_counts.motor3) * turn_sign;
  travel[3] = (int32_t)((uint32_t)current.motor4 - (uint32_t)start_counts.motor4) * turn_sign;
  *minimum = travel[0];
  for (i = 0; i < 4; ++i)
  {
    sum += travel[i];
    if (travel[i] < *minimum) *minimum = travel[i];
  }
  achieved_mdeg = (int32_t)(sum * LINE_SEARCH_CPS_DENOMINATOR /
      (4LL * LINE_SEARCH_EFFECTIVE_TRACK_MM * LINE_SEARCH_COUNTS_PER_REV)) * turn_sign;
  return sum;
}

static void command_turn(void)
{
  int32_t left = -turn_sign * turn_cps;
  DriveBase_PrepareLineTurnAssist(left, -left);
  DriveBase_SetSideCps(left, -left);
}

static void fail(uint8_t drive_fault)
{
  fault_mask = (drive_fault & 0x0fU) != 0U ?
      (drive_fault & 0x0fU) : BYPASS_TURN_CONTROLLER_FAULT;
  DriveBase_Stop(DRIVE_STOP_BRAKE);
  settling = 0U;
  state = LINE_BYPASS_TURN_FAULT;
}

uint8_t LineBypassTurn_Start(int32_t angle_mdeg, int32_t cps)
{
  DriveBaseTelemetry drive;
  int64_t angle = angle_mdeg;
  if (angle < 0) angle = -angle;
  /* Reject unsupported requests rather than silently increasing their speed.
     Default KEY1 is 1800 CPS, above the continuous-drive operating floor. */
  if (!angle || angle > 360000 || cps < 1412 || cps > 3600 ||
      state == LINE_BYPASS_TURN_RUNNING) return 0U;
  DriveBase_GetTelemetry(&drive);
  if (drive.fault_mask || drive.mode != DRIVE_BASE_STOPPED) return 0U;
  target_counts = (int32_t)((angle * LINE_SEARCH_EFFECTIVE_TRACK_MM *
      LINE_SEARCH_COUNTS_PER_REV + LINE_SEARCH_CPS_DENOMINATOR / 2) /
      LINE_SEARCH_CPS_DENOMINATOR);
  if (!target_counts) return 0U;
  turn_sign = angle_mdeg > 0 ? 1 : -1;
  turn_cps = cps;
  achieved_mdeg = 0;
  fault_mask = settling = 0U;
  started_ms = HAL_GetTick();
  timeout_ms = (uint32_t)((int64_t)target_counts * 3000 / cps) + 1500U;
  if (timeout_ms < 2500U) timeout_ms = 2500U;
  WheelEncoder_Start();
  WheelEncoder_GetCounts(&start_counts);
  /* This bounded encoder-guided action needs usable feedback. It must not
     inherit the line-search policy that observes encoder faults and runs on. */
  DriveBase_SetLineFaultObservation(0U, 0U, 0U);
  state = LINE_BYPASS_TURN_RUNNING;
  command_turn();
  return 1U;
}

uint8_t LineBypassTurn_RequestStop(void)
{
  if (state != LINE_BYPASS_TURN_RUNNING) return 0U;
  if (!settling)
  {
    DriveBase_Stop(DRIVE_STOP_BRAKE);
    stopped_ms = HAL_GetTick();
    settling = 1U;
  }
  return 1U;
}

void LineBypassTurn_Task(void)
{
  DriveBaseTelemetry drive;
  int32_t minimum;
  int64_t sum;
  uint32_t now = HAL_GetTick();
  if (state != LINE_BYPASS_TURN_RUNNING) return;
  DriveBase_Task(now);
  sum = travel_sum(&minimum);
  DriveBase_GetTelemetry(&drive);
  if (drive.fault_mask) { fail(drive.fault_mask); return; }
  if (settling)
  {
    if (now - stopped_ms >= BYPASS_TURN_SETTLE_MS &&
        drive.mode == DRIVE_BASE_STOPPED)
    {
      settling = 0U;
      state = LINE_BYPASS_TURN_DONE;
    }
    else if (now - stopped_ms >= BYPASS_TURN_SETTLE_LIMIT_MS) fail(0U);
    return;
  }
  /* An external stop/mode change must never be overwritten by a stale task. */
  if (drive.mode != DRIVE_BASE_SPEED) { fail(0U); return; }
  /* Keep all four wheels powered together until the average travel reaches
     the step; a stopped wheel cannot be hidden by the other three racing.
     No per-wheel endpoint pulses, reversals or low-speed tail corrections. */
  if (sum >= (int64_t)target_counts * 4 && minimum >= target_counts / 2)
  {
    (void)LineBypassTurn_RequestStop();
    return;
  }
  if (now - started_ms >= timeout_ms) { fail(0U); return; }
  command_turn();
}

void LineBypassTurn_Stop(void)
{
  if (state == LINE_BYPASS_TURN_RUNNING) DriveBase_Stop(DRIVE_STOP_COAST);
  state = LINE_BYPASS_TURN_IDLE;
  settling = fault_mask = 0U;
}

LineBypassTurnState LineBypassTurn_GetState(void) { return state; }
uint8_t LineBypassTurn_GetFaultMask(void) { return fault_mask; }
int32_t LineBypassTurn_GetAchievedAngleMdeg(void) { return achieved_mdeg; }
#endif
