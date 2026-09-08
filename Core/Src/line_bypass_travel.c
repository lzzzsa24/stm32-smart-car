#include "line_bypass_travel.h"
#include "drive_base.h"
#include "main.h"
#include "vehicle_geometry.h"
#include "wheel_encoder.h"

#define TRAVEL_COUNTS_PER_REV 1040L
#define TRAVEL_PI_X10000 31416L
#define TRAVEL_SETTLE_MS 120U
#define TRAVEL_SETTLE_LIMIT_MS 300U
#define TRAVEL_CONTROLLER_FAULT 0x10U
#define TRAVEL_CONTINUOUS_MAX_CPS 1800L

static LineBypassTravelState state;
static WheelEncoderCounts start;
static int32_t target_counts, signed_cps, direction;
static uint32_t progress_mm, started_ms, timeout_ms, stopped_ms;
static uint8_t settling, fault_mask;

static int64_t observe_travel(int32_t *minimum)
{
  WheelEncoderCounts current;
  int32_t travel[4];
  int64_t sum = 0;
  unsigned w;
  WheelEncoder_GetCounts(&current);
  travel[0] = (int32_t)((uint32_t)current.motor1 - (uint32_t)start.motor1);
  travel[1] = (int32_t)((uint32_t)current.motor2 - (uint32_t)start.motor2);
  travel[2] = (int32_t)((uint32_t)current.motor3 - (uint32_t)start.motor3);
  travel[3] = (int32_t)((uint32_t)current.motor4 - (uint32_t)start.motor4);
  *minimum = INT32_MAX;
  for(w=0;w<4;++w)
  {
    int32_t forward = direction > 0 ? travel[w] :
        (int32_t)(0U - (uint32_t)travel[w]);
    if(forward < *minimum) *minimum = forward;
    sum += forward;
  }
  /* Count signed wheel travel, including the whole-car braking interval.
     Backward slip cannot be counted as forward clearance. */
  progress_mm = sum > 0 ? (uint32_t)((sum * TRAVEL_PI_X10000 *
      VEHICLE_WHEEL_DIAMETER_MM + 2LL * TRAVEL_COUNTS_PER_REV * 10000) /
      (4LL * TRAVEL_COUNTS_PER_REV * 10000)) : 0U;
  return sum;
}

static void fail(uint8_t drive_fault)
{
  fault_mask = (drive_fault & 0x0fU) ? (drive_fault & 0x0fU) : TRAVEL_CONTROLLER_FAULT;
  DriveBase_Stop(DRIVE_STOP_BRAKE);
  settling = 0U;
  state = LINE_BYPASS_TRAVEL_FAULT;
}

uint8_t LineBypassTravel_Start(int32_t distance_mm, int32_t cps)
{
  DriveBaseTelemetry drive;
  int64_t distance = distance_mm;
  if(distance < 0) distance = -distance;
  if(!distance || distance > 65535 || cps < 1400 || cps > 3600 ||
      state == LINE_BYPASS_TRAVEL_RUNNING) return 0U;
  DriveBase_GetTelemetry(&drive);
  if(drive.fault_mask || drive.mode != DRIVE_BASE_STOPPED) return 0U;
  /* Short bypass legs should creep continuously, not accelerate toward a
     fast cruise then enter endpoint pulses. Keep the ordinary speed PI. */
  if(cps > TRAVEL_CONTINUOUS_MAX_CPS) cps = TRAVEL_CONTINUOUS_MAX_CPS;
  target_counts = (int32_t)((distance * TRAVEL_COUNTS_PER_REV * 10000 +
      TRAVEL_PI_X10000 * VEHICLE_WHEEL_DIAMETER_MM / 2) /
      (TRAVEL_PI_X10000 * VEHICLE_WHEEL_DIAMETER_MM));
  direction = distance_mm > 0 ? 1 : -1;
  signed_cps = direction * cps;
  timeout_ms = (uint32_t)((int64_t)target_counts * 4000 / cps) + 1200U;
  if(timeout_ms < 2000U) timeout_ms = 2000U;
  started_ms = HAL_GetTick();
  progress_mm = 0U;
  fault_mask = settling = 0U;
  WheelEncoder_Start();
  WheelEncoder_GetCounts(&start);
  /* Distance-based bypass needs working encoders, just like its turn owner. */
  DriveBase_SetLineFaultObservation(0U, 0U, 0U);
  state = LINE_BYPASS_TRAVEL_RUNNING;
  DriveBase_SetSideCps(signed_cps, signed_cps);
  return 1U;
}

void LineBypassTravel_Task(void)
{
  DriveBaseTelemetry drive;
  int32_t minimum;
  int64_t sum;
  uint32_t now = HAL_GetTick();
  if(state != LINE_BYPASS_TRAVEL_RUNNING) return;
  DriveBase_Task(now);
  sum = observe_travel(&minimum);
  DriveBase_GetTelemetry(&drive);
  if(drive.fault_mask) { fail(drive.fault_mask); return; }
  if(settling)
  {
    if(now - stopped_ms >= TRAVEL_SETTLE_MS && drive.mode == DRIVE_BASE_STOPPED)
    { settling = 0U; state = LINE_BYPASS_TRAVEL_DONE; }
    else if(now - stopped_ms >= TRAVEL_SETTLE_LIMIT_MS) fail(0U);
    return;
  }
  /* Never restart after an external STOP or replace another motion owner. */
  if(drive.mode != DRIVE_BASE_SPEED) { fail(0U); return; }
  if(sum >= 4LL * target_counts && minimum >= target_counts * 3 / 4)
  {
    DriveBase_Stop(DRIVE_STOP_BRAKE);
    stopped_ms = now;
    settling = 1U;
    return;
  }
  if(now - started_ms >= timeout_ms) { fail(0U); return; }
  DriveBase_SetSideCps(signed_cps, signed_cps);
}

void LineBypassTravel_Stop(void)
{
  if(state == LINE_BYPASS_TRAVEL_RUNNING) DriveBase_Stop(DRIVE_STOP_COAST);
  state = LINE_BYPASS_TRAVEL_IDLE;
  settling = fault_mask = 0U;
}
LineBypassTravelState LineBypassTravel_GetState(void) { return state; }
uint8_t LineBypassTravel_GetFaultMask(void) { return fault_mask; }
uint32_t LineBypassTravel_GetProgressMm(void) { return progress_mm; }
