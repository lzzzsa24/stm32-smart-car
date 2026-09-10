#ifndef Promoted_SIGN_LINE_FOLLOW_H
#define Promoted_SIGN_LINE_FOLLOW_H
#include "promoted_line_tracking.h"
#include "promoted_simple_line_mode.h"

typedef struct
{
  Promoted_SimpleLineController guard;
  uint8_t running, override_active, observation_paused;
  uint8_t observation_cycle;
  int8_t observation_search_direction;
  uint8_t arc_tracking_active;
  int8_t arc_steer_direction;
  uint8_t last_owner;
  uint8_t last_line_action;
} Promoted_SignLineFollowController;

typedef enum
{
  Promoted_SIGN_FOLLOW_OWNER_STOP = 0U,
  Promoted_SIGN_FOLLOW_OWNER_LINE,
  Promoted_SIGN_FOLLOW_OWNER_ROUTE,
  Promoted_SIGN_FOLLOW_OWNER_GUARD,
  Promoted_SIGN_FOLLOW_OWNER_OBSERVATION,
  Promoted_SIGN_FOLLOW_OWNER_CENTERING,
  Promoted_SIGN_FOLLOW_OWNER_ARC_FALLBACK
} SignFollowOwner;

void Promoted_SignLineFollow_Init(Promoted_SignLineFollowController *controller);
void Promoted_SignLineFollow_Start(Promoted_SignLineFollowController *controller);
void Promoted_SignLineFollow_Stop(Promoted_SignLineFollowController *controller);
/* Route state and yaw are observed once before this call. Only the selected
   owner may calculate/apply commands; shared recovery can itself drive motors. */
uint8_t Promoted_SignLineFollow_Step(Promoted_SignLineFollowController *controller,
    const Promoted_LineTrackingReading *reading, int16_t base_speed,
    const Promoted_SignRouteStatus *route, const Promoted_SignRouteCommand *route_command, uint8_t paused);
#endif
