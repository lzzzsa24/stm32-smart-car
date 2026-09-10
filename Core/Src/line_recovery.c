#include "line_recovery.h"
#include "line_search_model.h"
#include "drive_base.h"

#define SENSOR_CONFIRM_MS         4U
#define SENSOR_MAX_SAMPLE_GAP_MS 30U
#define EXIT_HINT_MAX_AGE_MS    200U
#define UNCERTAIN_SWEEP_START_MDEG 30000L
#define UNCERTAIN_SWEEP_STEP_MDEG  30000L
#define UNCERTAIN_SWEEP_MAX_MDEG  120000L

typedef enum { REC_IDLE, REC_SEARCH,
               REC_CAPTURED, REC_FAULT } RecoveryPhase;
static RecoveryPhase phase;
static int8_t side;
static int8_t exit_side;
static uint8_t exit_edge_seen;
static uint32_t exit_last_ms;
static uint8_t center_candidate;
static uint32_t center_since, center_last_ms;
static LineRecoveryStopReason stop_reason;
static uint8_t uncertain_search;
static int32_t uncertain_sweep_mdeg;
static uint32_t uncertain_start_transitions[DRIVE_BASE_WHEEL_COUNT];

static uint32_t uncertain_target_counts(void)
{
  return (uint32_t)(((int64_t)uncertain_sweep_mdeg *
      LINE_SEARCH_EFFECTIVE_TRACK_MM * LINE_SEARCH_COUNTS_PER_REV +
      LINE_SEARCH_CPS_DENOMINATOR / 2LL) / LINE_SEARCH_CPS_DENOMINATOR);
}

static void uncertain_snapshot(const DriveBaseTelemetry *telemetry)
{
  unsigned wheel;
  for (wheel = 0U; wheel < DRIVE_BASE_WHEEL_COUNT; ++wheel)
    uncertain_start_transitions[wheel] = telemetry->legal_transition_count[wheel];
}

static uint8_t uncertain_sweep_complete(const DriveBaseTelemetry *telemetry)
{
  uint32_t target = uncertain_target_counts();
  unsigned wheel;
  if (target == 0U) return 0U;
  for (wheel = 0U; wheel < DRIVE_BASE_WHEEL_COUNT; ++wheel)
  {
    if ((uint32_t)(telemetry->legal_transition_count[wheel] -
        uncertain_start_transitions[wheel]) < target) return 0U;
  }
  return 1U;
}

uint8_t LineRecovery_IsSearching(void) { return phase == REC_SEARCH; }
LineRecoveryStopReason LineRecovery_GetStopReason(void) { return stop_reason; }
int8_t LineRecovery_GetDirection(void) { return side; }
void LineRecovery_Stop(LineRecoveryStopReason reason)
{
  DriveBase_Stop(DRIVE_STOP_COAST);
  uncertain_search = 0U;
  stop_reason = reason;
  phase = REC_FAULT;
}
void LineRecovery_Reset(void)
{
  if (phase != REC_IDLE) DriveBase_Stop(DRIVE_STOP_COAST);
  phase = REC_IDLE;
  stop_reason = LINE_REC_STOP_NONE;
  center_candidate = 0U;
  side = exit_side = 0;
  exit_edge_seen = 0U;
  uncertain_search = 0U;
}
void LineRecovery_Commit(void)
{
  phase = REC_IDLE;
  stop_reason = LINE_REC_STOP_NONE;
  exit_edge_seen = 0U;
  uncertain_search = 0U;
}
void LineRecovery_Begin(int8_t preferred_side, uint32_t now)
{
  side = preferred_side > 0 ? 1 : -1;
  exit_side = side;
  /* A fresh outer observation supplies the pending exit, even after a long
     search. Do not treat the preferred/default side as a new sensor hit. */
  exit_edge_seen = 0U;
  exit_last_ms = now;
  center_candidate = 0U;
  uncertain_search = 0U;
  stop_reason = LINE_REC_STOP_NONE;
  /* Rolling handoff: preserve controller effort and let DriveBase ramp only
     the wheels that need to reverse. No whole-car brake/settle cycle. */
  phase = REC_SEARCH;
  center_last_ms = now;
  if (DriveBase_GetFaultMask()) LineRecovery_Stop(LINE_REC_STOP_DRIVE_FAULT);

}
void LineRecovery_BeginAmbiguous(int8_t initial_side, uint32_t now)
{
  DriveBaseTelemetry telemetry;
  LineRecovery_Begin(initial_side, now);
  if (phase != REC_SEARCH) return;
  DriveBase_GetTelemetry(&telemetry);
  uncertain_search = 1U;
  uncertain_sweep_mdeg = UNCERTAIN_SWEEP_START_MDEG;
  uncertain_snapshot(&telemetry);
}
void LineRecovery_ObserveDirection(const LineTrackingReading *r, uint32_t now)
{
  int8_t edge = r->x2_black && !r->x3_black && !r->x4_black ? -1 :
      (r->x4_black && !r->x1_black && !r->x2_black ? 1 : 0);
  if (phase != REC_SEARCH) return;
  /* History may invalidate live capture continuity, but must never complete
     capture on its own. A white/outer ISR hit between two live middle hits
     breaks the confirmation even if the main loop missed that hit. */
  if (!(r->x1_black || r->x3_black) || r->x2_black || r->x4_black)
    center_candidate = 0U;
  /* Every fresh unambiguous outer edge is evidence, not just the first edge
     after a middle hit. Switching still waits for its subsequent all-white
     exit, so contact alone does not reverse the active turn. */
  if (edge)
  {
    exit_side = edge;
    exit_edge_seen = 1U;
    exit_last_ms = now;
  }
  else if (r->x2_black || r->x4_black)
  {
    /* Conflicting wide/outer evidence invalidates the pending exit. An inner
       contact is still provisional until live capture confirms, so it must
       not discard a fresh outer direction or renew that direction's age. */
    exit_edge_seen = 0U;
  }
  else if (!(r->x1_black || r->x3_black) && exit_edge_seen)
  {
    if (now - exit_last_ms <= EXIT_HINT_MAX_AGE_MS)
    {
      side = exit_side;
      uncertain_search = 0U;
    }
    exit_edge_seen = 0U;
  }
}
static LineRecoveryResult step_recovery(const LineTrackingReading *r,
                                     LineTrackingCommand *command, uint32_t now,
                                     uint8_t immediate_capture)
{
  DriveBaseTelemetry telemetry;
  uint8_t visible = (r->x1_black || r->x3_black) && !r->x2_black && !r->x4_black;
  if (immediate_capture==3U) visible=0U; /* observation owns stricter capture */
  command->valid = 0U;
  command->left_cps = command->right_cps = 0;
  command->action = side < 0 ? LINE_ACTION_SEARCH_LEFT : LINE_ACTION_SEARCH_RIGHT;
  DriveBase_Task(now);
  if (DriveBase_GetFaultMask()) LineRecovery_Stop(LINE_REC_STOP_DRIVE_FAULT);
  if (phase == REC_FAULT) return LINE_RECOVERY_FAILED;
  if (phase == REC_CAPTURED) return LINE_RECOVERY_CAPTURED;
  LineRecovery_ObserveDirection(r, now);
  DriveBase_GetTelemetry(&telemetry);

  /* An externally requested brake retains ownership; removing our own entry
     brake must not cancel another controller's brake or capture through it. */
  if (telemetry.mode == DRIVE_BASE_BRAKING) return LINE_RECOVERY_BUSY;
  if (phase == REC_SEARCH)
  {
    LineRecovery_ObserveDirection(r, now);
    if (uncertain_search &&
        !(r->x1_black || r->x2_black || r->x3_black || r->x4_black) &&
        uncertain_sweep_complete(&telemetry))
    {
      side = (int8_t)-side;
      if (uncertain_sweep_mdeg < UNCERTAIN_SWEEP_MAX_MDEG)
      {
        uncertain_sweep_mdeg += UNCERTAIN_SWEEP_STEP_MDEG;
        if (uncertain_sweep_mdeg > UNCERTAIN_SWEEP_MAX_MDEG)
          uncertain_sweep_mdeg = UNCERTAIN_SWEEP_MAX_MDEG;
      }
      uncertain_snapshot(&telemetry);
    }
    command->action = side < 0 ? LINE_ACTION_SEARCH_LEFT : LINE_ACTION_SEARCH_RIGHT;
    if (!visible) center_candidate = 0U;
    else
    {
      /* Require repeated nearby observations, not one isolated sample or
         an assumed 20-ms-wide stripe. Switch to rolling capture immediately. */
      if (immediate_capture == 1U || (center_candidate &&
          now - center_last_ms <= SENSOR_MAX_SAMPLE_GAP_MS &&
          (immediate_capture == 2U ? now != center_last_ms : now - center_since >= SENSOR_CONFIRM_MS)))
      {
        phase = REC_CAPTURED;
        exit_edge_seen = 0U;
        return LINE_RECOVERY_CAPTURED;
      }
      if (!center_candidate || now - center_last_ms > SENSOR_MAX_SAMPLE_GAP_MS)
      { center_candidate = 1U; center_since = now; }
      center_last_ms = now;
    }
  }
  if (phase == REC_SEARCH && (immediate_capture==3U ||
      !(r->x1_black || r->x2_black || r->x3_black || r->x4_black)))
  {
    /* Only observation-centering opts into the existing KEY2 slow target.
       Keep the assist claim matched to the final four-wheel closed-loop CPS. */
    int32_t target = immediate_capture==3U ? LINE_TRACKING_MIDDLE_GUARD_CPS : LINE_SEARCH_TARGET_CPS;
    int32_t left = side < 0 ? -target : target;
    DriveBase_PrepareLineTurnAssist(left, -left);
    DriveBase_SetWheelCps(left, left, -left, -left);
  }
  return LINE_RECOVERY_BUSY;
}

LineRecoveryResult LineRecovery_Step(const LineTrackingReading *r,
                                     LineTrackingCommand *command, uint32_t now)
{ return step_recovery(r, command, now, 0U); }

LineRecoveryResult LineRecovery_StepImmediate(const LineTrackingReading *r,
                                             LineTrackingCommand *command, uint32_t now)
{ return step_recovery(r, command, now, 1U); }

LineRecoveryResult LineRecovery_StepRolling(const LineTrackingReading *r,
                                           LineTrackingCommand *command, uint32_t now)
{ return step_recovery(r, command, now, 2U); }

LineRecoveryResult LineRecovery_StepCentering(const LineTrackingReading *r,
                                             LineTrackingCommand *command, uint32_t now)
{ return step_recovery(r, command, now, 3U); }
