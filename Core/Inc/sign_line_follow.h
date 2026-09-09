#ifndef SIGN_LINE_FOLLOW_H
#define SIGN_LINE_FOLLOW_H
#include "line_tracking.h"
#include "simple_line_mode.h"

typedef struct
{
  SimpleLineController guard;
  uint8_t running, override_active;
} SignLineFollowController;

void SignLineFollow_Init(SignLineFollowController *controller);
void SignLineFollow_Start(SignLineFollowController *controller);
void SignLineFollow_Stop(SignLineFollowController *controller);
/* Route state and yaw are observed once before this call. Only the selected
   owner may calculate/apply commands; shared recovery can itself drive motors. */
uint8_t SignLineFollow_Step(SignLineFollowController *controller,
    const LineTrackingReading *reading, int16_t base_speed,
    const SignRouteStatus *route, const SignRouteCommand *route_command, uint8_t paused);
#endif
