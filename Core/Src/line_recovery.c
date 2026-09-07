#include "line_recovery.h"
#include "line_search_model.h"
#include "drive_base.h"
#include "buzzer_phrase_40077493715.h"

#define SENSOR_CONFIRM_MS         4U
#define SENSOR_MAX_SAMPLE_GAP_MS 30U
#define EXIT_HINT_MAX_AGE_MS    200U

typedef enum { REC_IDLE, REC_SEARCH,
               REC_CAPTURED, REC_FAULT } RecoveryPhase;
static RecoveryPhase phase;
static int8_t side;
static int8_t exit_side;
static uint8_t exit_edge_seen;
static uint32_t exit_last_ms;
static uint8_t center_candidate, audio_owned, audio_requested;
static uint32_t center_since, center_last_ms;
static LineRecoveryStopReason stop_reason;

static void stop_audio(void)
{
  if (audio_owned) BuzzerPhrase400_Stop();
  audio_owned = 0U;
  audio_requested = 0U;
}
static void search_audio(uint32_t now)
{
  BuzzerPhrase400_Task(now);
  if (!BuzzerPhrase400_IsPlaying())
  {
    audio_owned = BuzzerPhrase400_Start(1U);
  }
}
LineRecoveryStopReason LineRecovery_GetStopReason(void) { return stop_reason; }
int8_t LineRecovery_GetDirection(void) { return side; }
void LineRecovery_Stop(LineRecoveryStopReason reason)
{
  DriveBase_Stop(DRIVE_STOP_COAST);
  stop_audio();
  stop_reason = reason;
  phase = REC_FAULT;
}
void LineRecovery_Reset(void)
{
  if (phase != REC_IDLE) DriveBase_Stop(DRIVE_STOP_COAST);
  stop_audio();
  phase = REC_IDLE;
  stop_reason = LINE_REC_STOP_NONE;
  center_candidate = 0U;
  side = exit_side = 0;
  exit_edge_seen = 0U;
}
void LineRecovery_Commit(void)
{
  stop_audio();
  phase = REC_IDLE;
  stop_reason = LINE_REC_STOP_NONE;
  exit_edge_seen = 0U;
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
  stop_reason = LINE_REC_STOP_NONE;
  /* Rolling handoff: preserve controller effort and let DriveBase ramp only
     the wheels that need to reverse. No whole-car brake/settle cycle. */
  phase = REC_SEARCH;
  center_last_ms = now;
  if (DriveBase_GetFaultMask()) LineRecovery_Stop(LINE_REC_STOP_DRIVE_FAULT);
  else
  {
    /* Claim this phrase only for active line recovery. Normal manual audio
       remains untouched by reset/commit when recovery did not own it. */
    audio_owned = BuzzerPhrase400_Start(1U);
    audio_requested = 1U;
  }
}
void LineRecovery_BeginCorner(int8_t preferred_side, uint32_t now)
{
  side = preferred_side > 0 ? 1 : -1;
  exit_side = side;
  exit_edge_seen = 1U;
  exit_last_ms = now;
  center_candidate = 0U;
  stop_reason = LINE_REC_STOP_NONE;
  phase = REC_SEARCH;
  center_last_ms = now;
  audio_requested = 0U;
  if (DriveBase_GetFaultMask()) LineRecovery_Stop(LINE_REC_STOP_DRIVE_FAULT);
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
  else if (r->x1_black || r->x2_black || r->x3_black || r->x4_black)
  {
    /* A newer middle or ambiguous/wide observation supersedes the edge. */
    exit_edge_seen = 0U;
  }
  else if (exit_edge_seen)
  {
    if (now - exit_last_ms <= EXIT_HINT_MAX_AGE_MS) side = exit_side;
    exit_edge_seen = 0U;
  }
}
LineRecoveryResult LineRecovery_Step(const LineTrackingReading *r,
                                     LineTrackingCommand *command, uint32_t now)
{
  DriveBaseTelemetry telemetry;
  uint8_t visible = (r->x1_black || r->x3_black) && !r->x2_black && !r->x4_black;
  command->valid = 0U;
  command->left_cps = command->right_cps = 0;
  command->action = side < 0 ? LINE_ACTION_SEARCH_LEFT : LINE_ACTION_SEARCH_RIGHT;
  DriveBase_Task(now);
  if (DriveBase_GetFaultMask()) LineRecovery_Stop(LINE_REC_STOP_DRIVE_FAULT);
  if (phase == REC_FAULT) return LINE_RECOVERY_FAILED;
  if (phase == REC_CAPTURED) return LINE_RECOVERY_CAPTURED;
  LineRecovery_ObserveDirection(r, now);
  if (!(r->x1_black || r->x2_black || r->x3_black || r->x4_black)) audio_requested = 1U;
  if (audio_requested) search_audio(now);
  DriveBase_GetTelemetry(&telemetry);

  /* An externally requested brake retains ownership; removing our own entry
     brake must not cancel another controller's brake or capture through it. */
  if (telemetry.mode == DRIVE_BASE_BRAKING) return LINE_RECOVERY_BUSY;
  if (phase == REC_SEARCH)
  {
    LineRecovery_ObserveDirection(r, now);
    command->action = side < 0 ? LINE_ACTION_SEARCH_LEFT : LINE_ACTION_SEARCH_RIGHT;
    if (!visible) center_candidate = 0U;
    else
    {
      /* Require repeated nearby observations, not one isolated sample or
         an assumed 20-ms-wide stripe. Switch to rolling capture immediately. */
      if (!center_candidate || now - center_last_ms > SENSOR_MAX_SAMPLE_GAP_MS)
      { center_candidate = 1U; center_since = now; }
      else if (now - center_since >= SENSOR_CONFIRM_MS)
      {
        stop_audio();
        phase = REC_CAPTURED;
        return LINE_RECOVERY_CAPTURED;
      }
      center_last_ms = now;
    }
  }
  if (phase == REC_SEARCH)
  {
    int32_t left = side < 0 ? -LINE_SEARCH_TARGET_CPS : LINE_SEARCH_TARGET_CPS;
    DriveBase_PrepareLineTurnAssist(left, -left);
    DriveBase_SetWheelCps(left, left, -left, -left);
  }
  return LINE_RECOVERY_BUSY;
}
