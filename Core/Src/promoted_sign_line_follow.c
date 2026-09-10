#include "promoted_sign_line_follow.h"
#include "main.h"
#include "line_search_model.h"
#include "drive_base.h"
#include "motorPWM.h"
#include "promoted_line_recovery.h"
#include "promoted_sign_observation.h"
#include <string.h>

void Promoted_SignLineFollow_Init(Promoted_SignLineFollowController *c)
{
  memset(c, 0, sizeof(*c));
  Promoted_SimpleLine_Init(&c->guard);
}
void Promoted_SignLineFollow_Start(Promoted_SignLineFollowController *c)
{
  Promoted_SignLineFollow_Init(c);
  Promoted_SimpleLine_Start(&c->guard);
  Promoted_line_tracking_start_following(); /* identical to KEY2 */
  c->running = 1U;
}
void Promoted_SignLineFollow_Stop(Promoted_SignLineFollowController *c)
{
  if (!c->running) return;
  c->running = c->override_active = c->observation_paused = 0U;
  c->observation_cycle=c->arc_tracking_active=0U;
  c->arc_steer_direction=0;
  c->last_owner=Promoted_SIGN_FOLLOW_OWNER_STOP;
  c->last_line_action=Promoted_LINE_ACTION_STOP;
  Promoted_SimpleLine_Stop(&c->guard);
  Promoted_line_tracking_yield_to_route();
  DriveBase_Stop(DRIVE_STOP_COAST);
}

static uint8_t display_action(Promoted_LineTrackingAction action)
{
  if (action == Promoted_LINE_ACTION_STOP) return Promoted_SIMPLE_LINE_STOP;
  if (action == Promoted_LINE_ACTION_SEARCH_LEFT || action == Promoted_LINE_ACTION_SEARCH_RIGHT)
    return Promoted_SIMPLE_LINE_SEARCH;
  if (action == Promoted_LINE_ACTION_CROSSING) return Promoted_SIMPLE_LINE_WIDE;
  if (action == Promoted_LINE_ACTION_LEFT_SHARP || action == Promoted_LINE_ACTION_RIGHT_SHARP)
    return Promoted_SIMPLE_LINE_TURN;
  return Promoted_SIMPLE_LINE_TRACK;
}

static uint8_t symmetric_arc_contact(uint8_t mask)
{
  /* Both outer sensors or all four sensors do not describe a turn side.
     Keep the last real arc correction instead of driving toward the sign. */
  return mask == 9U || mask == 15U;
}

static void resume_observation(Promoted_SignLineFollowController *c)
{
  /* Keep the MPU sample just supplied by the caller, but drop the old
     filtered line/search sector. First resumed GPIO evidence owns recovery. */
  int64_t yaw=c->guard.yaw_mdeg;
  uint32_t generation=c->guard.yaw_generation;
  uint8_t configured=c->guard.yaw_configured, valid=c->guard.yaw_valid;
  Promoted_SimpleLine_Stop(&c->guard);
  Promoted_SimpleLine_Start(&c->guard);
  if (configured) Promoted_SimpleLine_UpdateYaw(&c->guard,yaw,valid,generation);
  Promoted_line_tracking_yield_to_route();
  c->observation_paused=c->override_active=0U;
}

uint8_t Promoted_SignLineFollow_Step(Promoted_SignLineFollowController *c,
    const Promoted_LineTrackingReading *reading, int16_t base_speed,
    const Promoted_SignRouteStatus *route, const Promoted_SignRouteCommand *route_command, uint8_t paused)
{
  Promoted_LineTrackingCommand output = {0};
  uint8_t mask = (uint8_t)((reading->x2_black ? 8U : 0U) |
      (reading->x1_black ? 4U : 0U) | (reading->x3_black ? 2U : 0U) |
      (reading->x4_black ? 1U : 0U));
  uint8_t arc = route->state == Promoted_SIGN_ROUTE_ARC;
  uint8_t mode3 = route->profile == Promoted_SIGN_ROUTE_PROFILE_STANDARD;
  uint8_t gyro_arc = arc && route->profile == Promoted_SIGN_ROUTE_PROFILE_GYRO_TANGENT;
  uint8_t exit_follow = route->profile == Promoted_SIGN_ROUTE_PROFILE_STANDARD &&
      route->state == Promoted_SIGN_ROUTE_EXIT_CLEAR;
  uint8_t guarded_search, override, action;
  uint8_t center_search=mode3 && Promoted_SignObservation_SeekingLine();

  if (!c->running || base_speed <= 0)
  {
    Promoted_SignLineFollow_Stop(c);
    return Promoted_SIMPLE_LINE_STOP;
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
    c->observation_search_direction=Promoted_LineRecovery_IsSearching() ?
        Promoted_LineRecovery_GetDirection() : c->guard.last_direction;
    c->observation_cycle=1U;
  }
  if (!paused && !center_search) c->observation_cycle=0U;
  if (paused)
  {
    if (!c->observation_paused)
    {
      Promoted_line_tracking_yield_to_route();
    }
    c->observation_paused=c->override_active=1U;
    c->last_owner=Promoted_SIGN_FOLLOW_OWNER_OBSERVATION;
    DriveBase_SetSpeedLimitCps(0L);
    DriveBase_SetLineFaultObservation(1U,mask,5U);
    output.valid=1U;
    output.action=Promoted_LINE_ACTION_FORWARD; /* same explicit zero targets as before */
    c->last_line_action=(uint8_t)output.action;
    Promoted_line_tracking_apply_command(&output,MOTOR_PWM_PERIOD);
    return 6U;
  }
  if (c->observation_paused) resume_observation(c);
  if (center_search)
  {
    uint32_t now=HAL_GetTick();
    if (!Promoted_LineRecovery_IsSearching())
    {
      Promoted_line_tracking_yield_to_route();
      Promoted_LineRecovery_Begin(c->observation_search_direction,now);
    }
    c->override_active=1U;
    c->last_owner=Promoted_SIGN_FOLLOW_OWNER_CENTERING;
    DriveBase_SetSpeedLimitCps(0L);
    DriveBase_SetLineFaultObservation(1U,mask,5U);
    (void)Promoted_LineRecovery_StepCentering(reading,&output,now);
    c->last_line_action=(uint8_t)output.action;
    return 7U; /* SEEK LINE; any middle contact is stopped by observation. */
  }
  /* SL2 supplies only the existing sign-entry/gyro search guard. Its visible
     line table is not used for ordinary or acquired-arc steering. */
  Promoted_SimpleLine_StepRoute(&c->guard, mask, route, route_command);
  guarded_search = c->guard.mode == Promoted_SIMPLE_LINE_SEARCH &&
      c->guard.entry_guard_active;
  override = route_command->active || guarded_search;
  if (override != c->override_active)
  {
    Promoted_line_tracking_yield_to_route();
    c->override_active = override;
  }

  /* Never carry a prior forward cap into shared recovery's signed targets. */
  DriveBase_SetSpeedLimitCps(0L);
  /* Reapply after route/observation yield resets the shared follower. This
     selection is idempotent and never resets live mode2 recovery each frame. */
  Promoted_line_tracking_set_middle_guard(mode3);

  if (override)
  {
    output.valid = 1U;
    output.action = Promoted_LINE_ACTION_FORWARD;
    DriveBase_SetLineFaultObservation(1U, mask, 5U);
    if (route_command->active)
    {
      c->last_owner=Promoted_SIGN_FOLLOW_OWNER_ROUTE;
      /* Route chooses heading; KEY2's slow rejoin profile chooses wheel CPS. */
      if (mode3 && route_command->heading_drive)
      {
        /* Forward leg immediately at the exit angle. Never let a middle/opposite
           ring contact resume ordinary following before outer completion.
           Use the existing encoder application and slow correction envelope. */
        int32_t error=route_command->drive_heading_error_mdeg;
        int32_t magnitude=error<0?-error:error;
        int32_t correction=magnitude>2000L?(magnitude-2000L)/50L:0L;
        int32_t cruise=Promoted_LINE_TRACKING_MIDDLE_GUARD_CPS;
        if (correction>cruise-1412L) correction=cruise-1412L;
        output.left_cps=cruise-(error<0?correction:0L);
        output.right_cps=cruise-(error>0?correction:0L);
        if (correction) output.action=error<0?
            Promoted_LINE_ACTION_LEFT_ADJUST:Promoted_LINE_ACTION_RIGHT_ADJUST;
      }
      else if (route_command->left_pwm > 0 || route_command->right_pwm > 0)
      {
        int8_t steer = route_command->left_pwm == route_command->right_pwm ? 0 :
            (route_command->left_pwm < route_command->right_pwm ? -1 : 1);
        if ((route_command->left_pwm < 0 && route_command->right_pwm > 0) ||
            (route_command->left_pwm > 0 && route_command->right_pwm < 0))
          Promoted_line_tracking_make_route_spin_command(steer, base_speed, &output);
        else if (route_command->gentle_arc)
          Promoted_line_tracking_make_slow_arc_command(steer, base_speed, &output);
        else Promoted_line_tracking_make_route_command(steer, base_speed, &output);
      }
      action = 5U;
    }
    else
    {
      c->last_owner=Promoted_SIGN_FOLLOW_OWNER_GUARD;
      /* Preserve the entry/arc yaw reversal bounds, with KEY2's
         calibrated encoder search target rather than SL2's 2700-PWM mapping. */
      output.left_cps = c->guard.left_pwm < 0 ? -LINE_SEARCH_TARGET_CPS :
          (c->guard.left_pwm > 0 ? LINE_SEARCH_TARGET_CPS : 0L);
      output.right_cps = -output.left_cps;
      action = Promoted_SIMPLE_LINE_SEARCH;
    }
  }
  else
  {
    /* Ordinary following/search resumes when route ownership is released. */
    Promoted_LineTrackingAction line_action;
    if (mode3)
    {
      /* Same compute/recovery/encoder application as comprehensive KEY2.
         Once on the arc, no separate gyro search owner can replace it. */
      line_action=Promoted_line_tracking_compute(reading,base_speed,&output);
      c->last_owner=Promoted_SIGN_FOLLOW_OWNER_LINE;
    }
    else if (gyro_arc && (mask == 0U || symmetric_arc_contact(mask)))
    {
      /* ARC has one continuous owner. A white gap or directionless broad
         contact uses the last forward curvature without route/guard takeover,
         so black/white flicker cannot reset the live follower every sample. */
      if (!c->arc_steer_direction && route->direction)
        c->arc_steer_direction=(int8_t)-route->direction;
      line_action=Promoted_line_tracking_compute_arc_fallback(reading,base_speed,
          c->arc_steer_direction,&output);
      c->last_owner=Promoted_SIGN_FOLLOW_OWNER_ARC_FALLBACK;
    }
    else
    {
      line_action = (arc || exit_follow) ?
          Promoted_line_tracking_compute_arc(reading, base_speed, &output) :
          Promoted_line_tracking_compute_slow(reading, base_speed, &output);
      c->last_owner=Promoted_SIGN_FOLLOW_OWNER_LINE;
      if (gyro_arc)
      {
        if (line_action==Promoted_LINE_ACTION_LEFT_ADJUST || line_action==Promoted_LINE_ACTION_LEFT_SHARP)
          c->arc_steer_direction=-1;
        else if (line_action==Promoted_LINE_ACTION_RIGHT_ADJUST || line_action==Promoted_LINE_ACTION_RIGHT_SHARP)
          c->arc_steer_direction=1;
      }
    }
    action = display_action(line_action);
    /* One slow straight target for bars, gaps, rejoin and exit travel, including
       commands supplied by recovery. No timed acceleration or recognition cap. */
    if (!mode3 && output.valid && (line_action == Promoted_LINE_ACTION_FORWARD || line_action == Promoted_LINE_ACTION_CROSSING))
    {
      Promoted_line_tracking_make_route_command(0, base_speed, &output);
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
  Promoted_line_tracking_apply_command(&output, MOTOR_PWM_PERIOD);
  return action;
}
