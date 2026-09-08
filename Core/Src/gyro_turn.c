#include "gyro_turn.h"
#include "mpu6050_yaw.h"
#include "drive_base.h"
#include "main.h"

static GyroTurnState state;
static uint8_t fault_code, settling, early_stop;
static int32_t target, sign, max_cps, achieved, progress_mark;
static int64_t start_yaw;
static uint32_t start_ms, timeout_ms, progress_ms, stop_ms, quiet_ms;
static uint8_t quiet;

static int32_t absolute(int32_t n) { return n < 0 ? -n : n; }
static void fail(uint8_t reason)
{
  DriveBase_Stop(DRIVE_STOP_COAST);
  fault_code = reason;
  state = GYRO_TURN_FAULT;
  settling = 0;
}
static void command(int32_t remaining)
{
  int32_t cps = 1412;
  if (remaining > 20000) cps = max_cps;
  else if (remaining > 0) cps += (max_cps - 1412) * remaining / 20000;
  DriveBase_PrepareLineTurnAssist(-sign * cps, sign * cps);
  DriveBase_SetSideCps(-sign * cps, sign * cps);
}
uint8_t GyroTurn_Start(int32_t angle_mdeg, int32_t maximum_cps)
{
  DriveBaseTelemetry drive;
  MpuYawReading imu;
  uint32_t now = HAL_GetTick();
  if (!angle_mdeg || angle_mdeg < -360000 || angle_mdeg > 360000 ||
      maximum_cps < 1412 || maximum_cps > 3600 ||
      state == GYRO_TURN_RUNNING || fault_code) return 0;
  DriveBase_GetTelemetry(&drive);
  if (drive.mode != DRIVE_BASE_STOPPED || drive.fault_mask) return 0;
  if (!MpuYaw_IsReady(now)) { fail(GYRO_TURN_SENSOR); return 0; }
  MpuYaw_GetReading(&imu);
  start_yaw = imu.yaw_mdeg;
  sign = angle_mdeg > 0 ? 1 : -1;
  target = absolute(angle_mdeg);
  max_cps = maximum_cps;
  achieved = progress_mark = 0;
  start_ms = progress_ms = now;
  timeout_ms = 2000U + (uint32_t)target / 30U;
  settling = early_stop = quiet = 0;
  DriveBase_SetLineFaultObservation(0, 0, 0);
  state = GYRO_TURN_RUNNING;
  command(target);
  return 1;
}
static void begin_settle(void)
{
  DriveBase_Stop(DRIVE_STOP_BRAKE);
  stop_ms = HAL_GetTick();
  quiet = 0;
  settling = 1;
}
uint8_t GyroTurn_RequestStop(void)
{
  if (state != GYRO_TURN_RUNNING) return 0;
  early_stop = 1;
  if (!settling) begin_settle();
  return 1;
}
void GyroTurn_Task(void)
{
  DriveBaseTelemetry drive;
  MpuYawReading imu;
  int64_t delta;
  int32_t along, remaining, lead;
  uint32_t now = HAL_GetTick();
  if (state != GYRO_TURN_RUNNING) return;
  /* Check sample freshness before allowing another drive command. */
  if (!MpuYaw_IsReady(now)) { fail(GYRO_TURN_SENSOR); return; }
  MpuYaw_GetReading(&imu);
  delta = imu.yaw_mdeg - start_yaw;
  if (delta < -720000 || delta > 720000) { fail(GYRO_TURN_SENSOR); return; }
  achieved = (int32_t)delta;
  along = achieved * sign;
  DriveBase_Task(now);
  DriveBase_GetTelemetry(&drive);
  if (drive.fault_mask) { fail(GYRO_TURN_DRIVE); return; }
  if (settling)
  {
    if (drive.mode != DRIVE_BASE_STOPPED && drive.mode != DRIVE_BASE_BRAKING)
    { fail(GYRO_TURN_DRIVE); return; }
    if (drive.mode == DRIVE_BASE_STOPPED && absolute(imu.rate_mdeg_s) < 8000)
    {
      if (!quiet) { quiet = 1; quiet_ms = now; }
      if (now - quiet_ms >= 120U)
      {
        if (!early_stop && absolute(target - along) > 4000)
        { fail(GYRO_TURN_ACCURACY); return; }
        state = GYRO_TURN_DONE;
        settling = 0;
      }
    }
    else quiet = 0;
    if (state == GYRO_TURN_RUNNING && now - stop_ms >= 700U)
      fail(GYRO_TURN_TIMEOUT);
    return;
  }
  if (drive.mode != DRIVE_BASE_SPEED) { fail(GYRO_TURN_DRIVE); return; }
  if (along < -3000) { fail(GYRO_TURN_DIRECTION); return; }
  if (along >= progress_mark + 1000) { progress_mark = along; progress_ms = now; }
  if (now - progress_ms >= 1200U) { fail(GYRO_TURN_NO_PROGRESS); return; }
  if (now - start_ms >= timeout_ms) { fail(GYRO_TURN_TIMEOUT); return; }
  remaining = target - along;
  /* Candidate 25 ms braking prediction, capped at 4 deg. Final settled yaw
     is checked; unacceptable overshoot is a fault, not false success. */
  lead = imu.rate_mdeg_s * sign / 40;
  if (lead < 500) lead = 500;
  if (lead > 4000) lead = 4000;
  if (remaining <= lead) { begin_settle(); return; }
  command(remaining);
}
void GyroTurn_Stop(void)
{
  if (state == GYRO_TURN_RUNNING) DriveBase_Stop(DRIVE_STOP_COAST);
  state = GYRO_TURN_IDLE;
  settling = 0;
}
uint8_t GyroTurn_ClearFault(void)
{
  DriveBaseTelemetry drive;
  DriveBase_GetTelemetry(&drive);
  if (state == GYRO_TURN_RUNNING || drive.mode != DRIVE_BASE_STOPPED) return 0;
  fault_code = 0;
  state = GYRO_TURN_IDLE;
  return 1;
}
uint8_t GyroTurn_GetFault(void) { return fault_code; }
GyroTurnState GyroTurn_GetState(void) { return state; }
int32_t GyroTurn_GetAchievedAngleMdeg(void) { return achieved; }
