#ifndef SIMPLE_LINE_MODE_H
#define SIMPLE_LINE_MODE_H

#include <stdint.h>
#include "sign_route.h"

#define SIMPLE_LINE_LEFT_OUTER  8U
#define SIMPLE_LINE_LEFT_INNER  4U
#define SIMPLE_LINE_RIGHT_INNER 2U
#define SIMPLE_LINE_RIGHT_OUTER 1U
#define SIMPLE_LINE_SEARCH_SECTOR_MDEG 25000L

typedef enum
{
  SIMPLE_LINE_STOP = 0,
  SIMPLE_LINE_TRACK,
  SIMPLE_LINE_TURN,
  SIMPLE_LINE_SEARCH,
  SIMPLE_LINE_WIDE
} SimpleLineMode;

typedef struct
{
  SimpleLineMode mode;
  uint8_t raw_mask;
  uint8_t filtered_mask;
  uint8_t candidate_mask;
  uint8_t sample_count;
  uint8_t ready;
  int8_t last_direction;
  uint8_t route_state;
  int8_t route_hint;
  int64_t yaw_mdeg, line_yaw_mdeg;
  uint32_t yaw_generation;
  uint8_t yaw_configured, yaw_valid, line_yaw_valid, sector_active;
  int8_t sector_direction;
  int16_t left_pwm;
  int16_t right_pwm;
} SimpleLineController;

void SimpleLine_Init(SimpleLineController *controller);
void SimpleLine_Start(SimpleLineController *controller);
void SimpleLine_Stop(SimpleLineController *controller);
void SimpleLine_SetDirection(SimpleLineController *controller,
                             int8_t direction);
void SimpleLine_Step(SimpleLineController *controller, uint8_t raw_mask);
/* Legacy standalone arc API; production sign modes use StepRoute. */
void SimpleLine_StepArc(SimpleLineController *controller, uint8_t raw_mask);
/* Mode 3/4 slow visible-line profile; white retains powered search. */
void SimpleLine_StepSlow(SimpleLineController *controller, uint8_t raw_mask);
/* One route hint per entry, followed by authoritative live line evidence. */
void SimpleLine_StepRoute(SimpleLineController *controller, uint8_t raw_mask,
                          const SignRouteStatus *route, const SignRouteCommand *command);
void SimpleLine_UpdateYaw(SimpleLineController *controller, int64_t yaw_mdeg,
                          uint8_t valid, uint32_t generation);

#endif
