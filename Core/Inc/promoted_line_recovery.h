#ifndef Promoted_LINE_RECOVERY_H
#define Promoted_LINE_RECOVERY_H
#include "promoted_line_tracking.h"

typedef enum { Promoted_LINE_RECOVERY_BUSY, Promoted_LINE_RECOVERY_CAPTURED,
               Promoted_LINE_RECOVERY_FAILED } Promoted_LineRecoveryResult;

typedef enum
{
  Promoted_LINE_REC_STOP_NONE = 0,
  Promoted_LINE_REC_STOP_DRIVE_FAULT = 9
} Promoted_LineRecoveryStopReason;

/* Preserved until a successful capture is committed or the mode is reset.
   Only drive faults latch; search has no timeout, distance or attempt limit. */
Promoted_LineRecoveryStopReason Promoted_LineRecovery_GetStopReason(void);
/* Current sensor-corrected side, -1 left / +1 right; zero after reset. */
int8_t Promoted_LineRecovery_GetDirection(void);
/* True until live capture/commit/stop. Independent of command validity/audio. */
uint8_t Promoted_LineRecovery_IsSearching(void);
/* Replay sampled direction evidence in main context, without motor/audio work.
   Unconfirmed inner contact preserves the pending outer exit until white,
   actual capture, contradictory outer evidence or its original age limit. */
void Promoted_LineRecovery_ObserveDirection(const Promoted_LineTrackingReading *reading, uint32_t now);
void Promoted_LineRecovery_Stop(Promoted_LineRecoveryStopReason reason);

void Promoted_LineRecovery_Reset(void);
/* Rolling loss entry: Step issues spin targets without an entry brake.
   DriveBase retains wheel-reversal ramping and externally owned braking. */
void Promoted_LineRecovery_Begin(int8_t preferred_side, uint32_t now);
/* A lone inner probe does not establish track direction. Start continuously,
   but reverse expanding encoder-bounded sweeps until stronger evidence or a
   middle capture is found. */
void Promoted_LineRecovery_BeginAmbiguous(int8_t initial_side, uint32_t now);
/* With visible line evidence Step only observes/captures; the tracking wrapper
   supplies forward steering. Only all-white search owns opposite wheel targets. */
Promoted_LineRecoveryResult Promoted_LineRecovery_Step(const Promoted_LineTrackingReading *reading,
                                     Promoted_LineTrackingCommand *command, uint32_t now);
/* Fast follower: current middle-only evidence captures without a time gate.
   External brake/fault ownership remains authoritative. No persistent setting. */
Promoted_LineRecoveryResult Promoted_LineRecovery_StepImmediate(const Promoted_LineTrackingReading *reading,
                                             Promoted_LineTrackingCommand *command, uint32_t now);
void Promoted_LineRecovery_Commit(void);
/* Opt-in mode3 observation search at KEY2's slow CPS, with normal DriveBase
   feedback/turn assistance. Caller owns middle-contact stopping. */
Promoted_LineRecoveryResult Promoted_LineRecovery_StepCentering(const Promoted_LineTrackingReading *reading,
                                             Promoted_LineTrackingCommand *command, uint32_t now);
/* Two distinct nearby live snapshots, with no stationary confirmation wait. */
Promoted_LineRecoveryResult Promoted_LineRecovery_StepRolling(const Promoted_LineTrackingReading *reading,
                                           Promoted_LineTrackingCommand *command, uint32_t now);
#endif
