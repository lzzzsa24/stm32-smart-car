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
  SIGN_ROUTE_CANCELLED    /* route withdrawn; line controller still runs */
} SignRouteState;

typedef struct
{
  uint8_t active;         /* visible-edge steering preference */
  uint8_t just_started;
  uint8_t just_finished;
  int8_t direction;       /* -1 left, +1 right */
  int16_t left_pwm;
  int16_t right_pwm;
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
} SignRouteStatus;

void SignRoute_Init(void);
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
