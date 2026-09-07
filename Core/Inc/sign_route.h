#ifndef SIGN_ROUTE_H
#define SIGN_ROUTE_H

#include <stdint.h>

#include "vision_detection.h"

typedef enum
{
  SIGN_ROUTE_IDLE = 0,
  SIGN_ROUTE_ARMED,
  SIGN_ROUTE_SELECTING,
  SIGN_ROUTE_LOCKED
} SignRouteState;

typedef struct
{
  uint8_t active;
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
} SignRouteStatus;

void SignRoute_Init(void);
void SignRoute_Reset(void);
void SignRoute_ObserveDetection(const VisionDetection *detection);
void SignRoute_Step(uint8_t line_mask,
                    uint32_t now,
                    SignRouteCommand *command);
void SignRoute_GetStatus(uint32_t now, SignRouteStatus *status);

#endif
