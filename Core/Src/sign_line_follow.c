#include "sign_line_follow.h"
#include "line_search_model.h"
#include "drive_base.h"
#include "motorPWM.h"
#include <string.h>

void SignLineFollow_Init(SignLineFollowController *c)
{
  memset(c, 0, sizeof(*c));
  SimpleLine_Init(&c->guard);
}
void SignLineFollow_Start(SignLineFollowController *c)
{
  SignLineFollow_Init(c);
  SimpleLine_Start(&c->guard);
  line_tracking_start_following(); /* identical to KEY2 */
  c->running = 1U;
}
void SignLineFollow_Stop(SignLineFollowController *c)
{
  if (!c->running) return;
  c->running = c->override_active = 0U;
  SimpleLine_Stop(&c->guard);
  line_tracking_yield_to_route();
  DriveBase_Stop(DRIVE_STOP_COAST);
}

static uint8_t display_action(LineTrackingAction action)
{
  if (action == LINE_ACTION_STOP) return SIMPLE_LINE_STOP;
  if (action == LINE_ACTION_SEARCH_LEFT || action == LINE_ACTION_SEARCH_RIGHT)
    return SIMPLE_LINE_SEARCH;
  if (action == LINE_ACTION_CROSSING) return SIMPLE_LINE_WIDE;
  if (action == LINE_ACTION_LEFT_SHARP || action == LINE_ACTION_RIGHT_SHARP)
    return SIMPLE_LINE_TURN;
  return SIMPLE_LINE_TRACK;
}

uint8_t SignLineFollow_Step(SignLineFollowController *c,
    const LineTrackingReading *reading, int16_t base_speed,
    const SignRouteStatus *route, const SignRouteCommand *route_command, uint8_t paused)
{
  LineTrackingCommand output = {0};
  uint8_t mask = (uint8_t)((reading->x2_black ? 8U : 0U) |
      (reading->x1_black ? 4U : 0U) | (reading->x3_black ? 2U : 0U) |
      (reading->x4_black ? 1U : 0U));
  uint8_t arc = route->state == SIGN_ROUTE_ARC;
  uint8_t guarded_search, override, action;

  if (!c->running || base_speed <= 0)
  {
    SignLineFollow_Stop(c);
    return SIMPLE_LINE_STOP;
  }
  /* SL2 supplies only the existing sign-entry/gyro search guard. Its visible
     line table is not used for ordinary or acquired-arc steering. */
  SimpleLine_StepRoute(&c->guard, mask, route, route_command);
  guarded_search = c->guard.mode == SIMPLE_LINE_SEARCH &&
      (c->guard.entry_guard_active || arc);
  override = paused || route_command->active || guarded_search;
  if (override != c->override_active)
  {
    line_tracking_yield_to_route();
    c->override_active = override;
  }

  /* Never carry a prior forward cap into shared recovery's signed targets. */
  DriveBase_SetSpeedLimitCps(0L);

  if (override)
  {
    output.valid = 1U;
    output.action = LINE_ACTION_FORWARD;
    DriveBase_SetLineFaultObservation(1U, mask, 5U);
    if (paused) action = 6U;
    else if (route_command->active)
    {
      /* Route chooses heading; KEY2's slow rejoin profile chooses wheel CPS. */
      if (route_command->left_pwm > 0 || route_command->right_pwm > 0)
      {
        int8_t steer = route_command->left_pwm == route_command->right_pwm ? 0 :
            (route_command->left_pwm < route_command->right_pwm ? -1 : 1);
        if (route_command->gentle_arc)
          line_tracking_make_slow_arc_command(steer, base_speed, &output);
        else line_tracking_make_route_command(steer, base_speed, &output);
      }
      action = 5U;
    }
    else
    {
      /* Preserve the selected half-sector/yaw reversal bounds, with KEY2's
         calibrated encoder search target rather than SL2's 2700-PWM mapping. */
      output.left_cps = c->guard.left_pwm < 0 ? -LINE_SEARCH_TARGET_CPS :
          (c->guard.left_pwm > 0 ? LINE_SEARCH_TARGET_CPS : 0L);
      output.right_cps = -output.left_cps;
      action = SIMPLE_LINE_SEARCH;
    }
  }
  else
  {
    LineTrackingAction line_action = line_tracking_compute_slow(reading, base_speed, &output);
    action = display_action(line_action);
    /* One slow straight target for bars, gaps, rejoin and exit travel, including
       commands supplied by recovery. No timed acceleration or recognition cap. */
    if (output.valid && (line_action == LINE_ACTION_FORWARD || line_action == LINE_ACTION_CROSSING))
    {
      line_tracking_make_route_command(0, base_speed, &output);
      output.action = line_action;
    }
    /* A captured semicircle has no hairpin. Retain KEY2's initial outer-probe
       pivot when its persistent-edge escalation would counter-rotate. */
    if (arc && (mask == 8U || mask == 1U) && output.valid &&
        ((output.left_cps < 0 && output.right_cps > 0) ||
         (output.left_cps > 0 && output.right_cps < 0)))
    {
      if (output.left_cps < 0) output.left_cps = 0L;
      if (output.right_cps < 0) output.right_cps = 0L;
    }
  }
  line_tracking_apply_command(&output, MOTOR_PWM_PERIOD);
  return action;
}
