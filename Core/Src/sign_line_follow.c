#include "sign_line_follow.h"
#include "main.h"
#include "line_search_model.h"
#include "drive_base.h"
#include "motorPWM.h"
#include "line_recovery.h"
#include "sign_observation.h"
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
  c->running = c->override_active = c->observation_paused = 0U;
  c->observation_cycle=c->arc_tracking_active=0U;
  c->arc_steer_direction=0;
  c->last_owner=SIGN_FOLLOW_OWNER_STOP;
  c->last_line_action=LINE_ACTION_STOP;
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

static uint8_t symmetric_arc_contact(uint8_t mask)
{
  /* Both outer sensors or all four sensors do not describe a turn side.
     Keep the last real arc correction instead of driving toward the sign. */
  return mask == 9U || mask == 15U;
}

static void resume_observation(SignLineFollowController *c)
{
  /* Keep the MPU sample just supplied by the caller, but drop the old
     filtered line/search sector. First resumed GPIO evidence owns recovery. */
  int64_t yaw=c->guard.yaw_mdeg;
  uint32_t generation=c->guard.yaw_generation;
  uint8_t configured=c->guard.yaw_configured, valid=c->guard.yaw_valid;
  SimpleLine_Stop(&c->guard);
  SimpleLine_Start(&c->guard);
  if (configured) SimpleLine_UpdateYaw(&c->guard,yaw,valid,generation);
  line_tracking_yield_to_route();
  c->observation_paused=c->override_active=0U;
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
  uint8_t mode3 = route->profile == SIGN_ROUTE_PROFILE_STANDARD;
  uint8_t gyro_arc = arc && route->profile == SIGN_ROUTE_PROFILE_GYRO_TANGENT;
  uint8_t exit_follow = route->profile == SIGN_ROUTE_PROFILE_STANDARD &&
      route->state == SIGN_ROUTE_EXIT_CLEAR;
  uint8_t guarded_search, override, action;
  uint8_t center_search=route->profile==SIGN_ROUTE_PROFILE_STANDARD &&
      SignObservation_SeekingLine();

  if (!c->running || base_speed <= 0)
  {
    SignLineFollow_Stop(c);
    return SIMPLE_LINE_STOP;
  }
  if (gyro_arc && !c->arc_tracking_active)
  {
    c->arc_tracking_active=1U;
    c->arc_steer_direction=route->direction ? (int8_t)-route->direction : 0;
  }
  else if (!gyro_arc)
  {
    c->arc_tracking_active=0U;
    c->arc_steer_direction=0;
  }
  if ((paused || center_search) && !c->observation_cycle)
  {
    c->observation_search_direction=LineRecovery_IsSearching() ?
        LineRecovery_GetDirection() : c->guard.last_direction;
    c->observation_cycle=1U;
  }
  if (!paused && !center_search) c->observation_cycle=0U;
  if (paused)
  {
    if (!c->observation_paused)
    {
      if (center_search && LineRecovery_IsSearching())
        c->observation_search_direction=LineRecovery_GetDirection();
      line_tracking_yield_to_route();
    }
    c->observation_paused=c->override_active=1U;
    c->last_owner=SIGN_FOLLOW_OWNER_OBSERVATION;
    DriveBase_SetSpeedLimitCps(0L);
    DriveBase_SetLineFaultObservation(1U,mask,5U);
    output.valid=1U;
    output.action=LINE_ACTION_FORWARD; /* same explicit zero targets as before */
    c->last_line_action=(uint8_t)output.action;
    line_tracking_apply_command(&output,MOTOR_PWM_PERIOD);
    return 6U;
  }
  if (c->observation_paused) resume_observation(c);
  if (center_search)
  {
    uint32_t now=HAL_GetTick();
    if (!LineRecovery_IsSearching())
    {
      line_tracking_yield_to_route();
      LineRecovery_Begin(c->observation_search_direction,now);
    }
    c->override_active=1U;
    c->last_owner=SIGN_FOLLOW_OWNER_CENTERING;
    DriveBase_SetSpeedLimitCps(0L);
    DriveBase_SetLineFaultObservation(1U,mask,5U);
    (void)LineRecovery_StepCentering(reading,&output,now);
    c->last_line_action=(uint8_t)output.action;
    return 7U; /* SEEK LINE; recovery owns the established encoder targets */
  }
  /* SL2 supplies only the existing sign-entry/gyro search guard. Its visible
     line table is not used for ordinary or acquired-arc steering. */
  SimpleLine_StepRoute(&c->guard, mask, route, route_command);
  guarded_search = c->guard.mode == SIMPLE_LINE_SEARCH &&
      c->guard.entry_guard_active;
  override = route_command->active || guarded_search;
  if (override != c->override_active)
  {
    line_tracking_yield_to_route();
    c->override_active = override;
  }

  /* Never carry a prior forward cap into shared recovery's signed targets. */
  DriveBase_SetSpeedLimitCps(0L);
  /* Reapply after route/observation yield resets the shared follower. This
     selection is idempotent and never resets live mode2 recovery each frame. */
  line_tracking_set_middle_guard(mode3);

  if (override)
  {
    output.valid = 1U;
    output.action = LINE_ACTION_FORWARD;
    DriveBase_SetLineFaultObservation(1U, mask, 5U);
    if (route_command->active)
    {
      c->last_owner=SIGN_FOLLOW_OWNER_ROUTE;
      /* Route chooses heading; KEY2's slow rejoin profile chooses wheel CPS. */
      if (route_command->left_pwm > 0 || route_command->right_pwm > 0)
      {
        int8_t steer = route_command->left_pwm == route_command->right_pwm ? 0 :
            (route_command->left_pwm < route_command->right_pwm ? -1 : 1);
        if ((route_command->left_pwm < 0 && route_command->right_pwm > 0) ||
            (route_command->left_pwm > 0 && route_command->right_pwm < 0))
          line_tracking_make_route_spin_command(steer, base_speed, &output);
        else if (route_command->gentle_arc)
          line_tracking_make_slow_arc_command(steer, base_speed, &output);
        else line_tracking_make_route_command(steer, base_speed, &output);
      }
      action = 5U;
    }
    else
    {
      c->last_owner=SIGN_FOLLOW_OWNER_GUARD;
      /* Preserve the entry/arc yaw reversal bounds, with KEY2's
         calibrated encoder search target rather than SL2's 2700-PWM mapping. */
      output.left_cps = c->guard.left_pwm < 0 ? -LINE_SEARCH_TARGET_CPS :
          (c->guard.left_pwm > 0 ? LINE_SEARCH_TARGET_CPS : 0L);
      output.right_cps = -output.left_cps;
      action = SIMPLE_LINE_SEARCH;
    }
  }
  else
  {
    /* Mode 3's aligned exit is already live tracking, with the same slow
       targets. An old crossing tail must not hide a current outer contact. */
    LineTrackingAction line_action;
    if (mode3)
    {
      /* Same compute/recovery/encoder application as comprehensive KEY2.
         Once on the arc, no separate gyro search owner can replace it. */
      line_action=line_tracking_compute(reading,base_speed,&output);
      c->last_owner=SIGN_FOLLOW_OWNER_LINE;
    }
    else if (gyro_arc && (mask == 0U || symmetric_arc_contact(mask)))
    {
      /* ARC has one continuous owner. A white gap or directionless broad
         contact uses the last forward curvature without route/guard takeover,
         so black/white flicker cannot reset the live follower every sample. */
      if (!c->arc_steer_direction && route->direction)
        c->arc_steer_direction=(int8_t)-route->direction;
      line_action=line_tracking_compute_arc_fallback(reading,base_speed,
          c->arc_steer_direction,&output);
      c->last_owner=SIGN_FOLLOW_OWNER_ARC_FALLBACK;
    }
    else
    {
      line_action = (arc || exit_follow) ?
          line_tracking_compute_arc(reading, base_speed, &output) :
          line_tracking_compute_slow(reading, base_speed, &output);
      c->last_owner=SIGN_FOLLOW_OWNER_LINE;
      if (gyro_arc)
      {
        if (line_action==LINE_ACTION_LEFT_ADJUST || line_action==LINE_ACTION_LEFT_SHARP)
          c->arc_steer_direction=-1;
        else if (line_action==LINE_ACTION_RIGHT_ADJUST || line_action==LINE_ACTION_RIGHT_SHARP)
          c->arc_steer_direction=1;
      }
    }
    action = display_action(line_action);
    /* One slow straight target for bars, gaps, rejoin and exit travel, including
       commands supplied by recovery. No timed acceleration or recognition cap. */
    if (!mode3 && output.valid && (line_action == LINE_ACTION_FORWARD || line_action == LINE_ACTION_CROSSING))
    {
      line_tracking_make_route_command(0, base_speed, &output);
      output.action = line_action;
    }
    /* A captured semicircle has no hairpin. Retain KEY2's initial outer-probe
       pivot when its persistent-edge escalation would counter-rotate. */
    if (gyro_arc && (mask == 8U || mask == 1U) && output.valid &&
        ((output.left_cps < 0 && output.right_cps > 0) ||
         (output.left_cps > 0 && output.right_cps < 0)))
    {
      if (output.left_cps < 0) output.left_cps = 0L;
      if (output.right_cps < 0) output.right_cps = 0L;
    }
  }
  c->last_line_action=(uint8_t)output.action;
  line_tracking_apply_command(&output, MOTOR_PWM_PERIOD);
  return action;
}
