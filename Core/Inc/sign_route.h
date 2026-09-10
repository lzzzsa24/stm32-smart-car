#ifndef SIGN_ROUTE_H
#define SIGN_ROUTE_H

#include <stdint.h>

#include "vision_detection.h"

typedef enum
{
  SIGN_ROUTE_IDLE = 0,
  SIGN_ROUTE_ARMED,
  SIGN_ROUTE_SELECTING,
  SIGN_ROUTE_LOCKED,
  SIGN_ROUTE_PROBE,
  SIGN_ROUTE_WAIT_SIGN,
  SIGN_ROUTE_ARC,
  SIGN_ROUTE_EXIT_SELECT,
  SIGN_ROUTE_EXIT_CLEAR,
  SIGN_ROUTE_FAULT,       /* reserved legacy state; no timed line-loss hold */
  SIGN_ROUTE_SEARCHING,   /* display-only; search keeps the underlying route phase */
  SIGN_ROUTE_CANCELLED,   /* route withdrawn; line controller still runs */
  SIGN_ROUTE_ENTRY_RETURN,
  SIGN_ROUTE_ENTRY_FALLBACK
} SignRouteState;

typedef enum
{
  SIGN_ROUTE_PROFILE_STANDARD = 0,
  SIGN_ROUTE_PROFILE_GYRO_TANGENT
} SignRouteProfile;

typedef struct
{
  uint8_t active;         /* route phase temporarily owns the wheel targets */
  uint8_t just_started;
  uint8_t just_finished;
  int8_t direction;       /* -1 left, +1 right */
  int16_t left_pwm;
  int16_t right_pwm;
  uint8_t gentle_arc;     /* both sides forward at KEY2 settle turn speeds */
} SignRouteCommand;

typedef struct
{
  SignRouteState state;
  int8_t direction;
  int8_t last_class;
  uint8_t last_score;
  uint8_t vision_online;
  uint32_t last_sequence;
  uint8_t searching;
  uint8_t yaw_valid;
  uint8_t entry_line_ready; /* selected outer -> stable center, releases entry preference */
  uint8_t fault;          /* navigation warnings only; line loss never owns STOP */
  int32_t travel_mm;
  int32_t yaw_mdeg;       /* phase-relative MPU yaw; mdeg, positive left */
  SignRouteProfile profile;
  int32_t heading_error_mdeg; /* current heading minus approach, wrapped +/-180 deg */
  int32_t arc_peak_mdeg;  /* maximum arc angle observed on a narrow track line */
  uint8_t approach_from_pause; /* completed observation is the primary immutable reference */
  int32_t exit_heading_peak_mdeg; /* signed upper-half peak on line, relative to straight reference */
  uint8_t road_reference_valid; /* valid stopped reference, or PROBE fallback if no stop exists */
  uint8_t exit_reason; /* 0 none, 1 angle/reacquisition, 2 natural return, 3 missed window */
  int32_t arc_sweep_mdeg; /* directed visible-line sweep, independent of road reference */
} SignRouteStatus;

void SignRoute_Init(void);
/* Mode 4 selects the gyro-tangent profile; mode 3 keeps STANDARD. */
void SignRoute_SetProfile(SignRouteProfile profile);
/* Mode 3 always captures the completed observation pose as its primary reference.
   Mode 4 starts its drawn trajectory here only with a confirmed direction. */
void SignRoute_UpdateObservationPause(uint8_t paused, uint32_t now);
/* Rebase pre-entry geometry when the centered mode-3 observation finishes. */
void SignRoute_MarkObservationSearch(void);
/* Fresh continuous MPU yaw: positive left, millidegrees. No motor ownership. */
void SignRoute_UpdateYaw(int64_t yaw_mdeg, uint8_t valid);
void SignRoute_Reset(void);
/* WheelEncoder logical signed cumulative counts (1040 per revolution).
   Call once each navigation cycle; individual counter wrap is handled. */
void SignRoute_UpdateEncoders(int32_t m1, int32_t m2, int32_t m3, int32_t m4);
void SignRoute_ObserveDetection(const VisionDetection *detection);
void SignRoute_Step(uint8_t line_mask,
                    uint32_t now,
                    SignRouteCommand *command);
void SignRoute_GetStatus(uint32_t now, SignRouteStatus *status);

#endif
