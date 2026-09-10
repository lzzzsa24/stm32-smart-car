#ifndef Promoted_SIMPLE_LINE_MODE_H
#define Promoted_SIMPLE_LINE_MODE_H

#include <stdint.h>
#include "promoted_sign_route.h"

#define Promoted_SIMPLE_LINE_LEFT_OUTER  8U
#define Promoted_SIMPLE_LINE_LEFT_INNER  4U
#define Promoted_SIMPLE_LINE_RIGHT_INNER 2U
#define Promoted_SIMPLE_LINE_RIGHT_OUTER 1U
#define Promoted_SIMPLE_LINE_SEARCH_SECTOR_MDEG 25000L
/* Entry approaches a quarter-turn; leave 10 degrees before the route's
   120-degree invalid-entry bound. Measured phase yaw anchors this sector. */
#define Promoted_SIMPLE_LINE_ENTRY_SEARCH_SECTOR_MDEG 110000L
#define Promoted_SIMPLE_LINE_ARC_TREND_MDEG 5000L

typedef enum
{
  Promoted_SIMPLE_LINE_STOP = 0,
  Promoted_SIMPLE_LINE_TRACK,
  Promoted_SIMPLE_LINE_TURN,
  Promoted_SIMPLE_LINE_SEARCH,
  Promoted_SIMPLE_LINE_WIDE
} Promoted_SimpleLineMode;

typedef struct
{
  Promoted_SimpleLineMode mode;
  uint8_t raw_mask;
  uint8_t filtered_mask;
  uint8_t candidate_mask;
  uint8_t sample_count;
  uint8_t ready;
  int8_t last_direction;
  uint8_t route_state;
  int8_t route_hint;
  int64_t yaw_mdeg, line_yaw_mdeg;
  int64_t curve_yaw_mdeg;
  uint32_t yaw_generation;
  uint8_t yaw_configured, yaw_valid, line_yaw_valid, sector_active;
  uint8_t entry_guard_active;
  uint8_t curve_yaw_valid;
  int8_t sector_direction;
  int16_t left_pwm;
  int16_t right_pwm;
} Promoted_SimpleLineController;

void Promoted_SimpleLine_Init(Promoted_SimpleLineController *controller);
void Promoted_SimpleLine_Start(Promoted_SimpleLineController *controller);
void Promoted_SimpleLine_Stop(Promoted_SimpleLineController *controller);
void Promoted_SimpleLine_SetDirection(Promoted_SimpleLineController *controller,
                             int8_t direction);
void Promoted_SimpleLine_Step(Promoted_SimpleLineController *controller, uint8_t raw_mask);
/* Legacy standalone arc API; production sign modes use StepRoute. */
void Promoted_SimpleLine_StepArc(Promoted_SimpleLineController *controller, uint8_t raw_mask);
/* Sign helper profile; production mode 3 uses shared tracking for visible line. */
void Promoted_SimpleLine_StepSlow(Promoted_SimpleLineController *controller, uint8_t raw_mask);
/* Protect uncompleted branch selection; follow live line after selected capture. */
void Promoted_SimpleLine_StepRoute(Promoted_SimpleLineController *controller, uint8_t raw_mask,
                          const Promoted_SignRouteStatus *route, const Promoted_SignRouteCommand *command);
void Promoted_SimpleLine_UpdateYaw(Promoted_SimpleLineController *controller, int64_t yaw_mdeg,
                          uint8_t valid, uint32_t generation);
/* Actual sign-mode output ownership: manual STOP, observation, route, follower. */
uint8_t Promoted_SimpleLine_ResolveRouteOutput(const Promoted_SimpleLineController *controller,
                                    const Promoted_SignRouteCommand *command, uint8_t paused,
                                    int16_t *left, int16_t *right);

#endif
