#ifndef LINE_RECOVERY_H
#define LINE_RECOVERY_H
#include "line_tracking.h"

typedef enum { LINE_RECOVERY_BUSY, LINE_RECOVERY_CAPTURED,
               LINE_RECOVERY_FAILED } LineRecoveryResult;

typedef enum
{
  LINE_REC_STOP_NONE = 0,
  LINE_REC_STOP_DRIVE_FAULT = 9
} LineRecoveryStopReason;

/* Preserved until a successful capture is committed or the mode is reset.
   Only drive faults latch; search has no timeout, distance or attempt limit. */
LineRecoveryStopReason LineRecovery_GetStopReason(void);
/* Current sensor-corrected side, -1 left / +1 right; zero after reset. */
int8_t LineRecovery_GetDirection(void);
/* True until live capture/commit/stop. Independent of command validity/audio. */
uint8_t LineRecovery_IsSearching(void);
/* Replay sampled direction evidence in main context, without motor/audio work.
   Unconfirmed inner contact preserves the pending outer exit until white,
   actual capture, contradictory outer evidence or its original age limit. */
void LineRecovery_ObserveDirection(const LineTrackingReading *reading, uint32_t now);
void LineRecovery_Stop(LineRecoveryStopReason reason);

void LineRecovery_Reset(void);
/* Rolling loss entry: Step issues spin targets without an entry brake.
   DriveBase retains wheel-reversal ramping and externally owned braking. */
void LineRecovery_Begin(int8_t preferred_side, uint32_t now);
/* A lone inner probe does not establish track direction. Start continuously,
   but reverse expanding encoder-bounded sweeps until stronger evidence or a
   middle capture is found. */
void LineRecovery_BeginAmbiguous(int8_t initial_side, uint32_t now);
/* With visible line evidence Step only observes/captures; the tracking wrapper
   supplies forward steering. Only all-white search owns opposite wheel targets. */
LineRecoveryResult LineRecovery_Step(const LineTrackingReading *reading,
                                     LineTrackingCommand *command, uint32_t now);
/* Fast follower: current middle-only evidence captures without a time gate.
   External brake/fault ownership remains authoritative. No persistent setting. */
LineRecoveryResult LineRecovery_StepImmediate(const LineTrackingReading *reading,
                                             LineTrackingCommand *command, uint32_t now);
void LineRecovery_Commit(void);
#endif
