#include "simple_line_mode.h"
#include "sign_route_config.h"

#include <stddef.h>
#include <string.h>

#define SIMPLE_LINE_FILTER_SAMPLES 2U
#define SIMPLE_LINE_STRAIGHT_PWM 2400
#define SIMPLE_LINE_SLOW_PWM     2200
#define SIMPLE_LINE_OUTER_PWM    2600
#define SIMPLE_LINE_TURN_PWM     2700

_Static_assert(SIMPLE_LINE_ENTRY_SEARCH_SECTOR_MDEG < SIGN_ENTRY_MAX_MDEG,
               "Entry search must reverse before the route yaw cancellation bound");

static void set_output(SimpleLineController *controller,
                       SimpleLineMode mode,
                       int16_t left_pwm,
                       int16_t right_pwm)
{
  controller->mode = mode;
  controller->left_pwm = left_pwm;
  controller->right_pwm = right_pwm;
}

static void set_turn(SimpleLineController *controller, int8_t direction)
{
  int16_t left = direction < 0 ? -SIMPLE_LINE_TURN_PWM :
                                 SIMPLE_LINE_TURN_PWM;
  set_output(controller, SIMPLE_LINE_TURN, left, (int16_t)-left);
}

void SimpleLine_Init(SimpleLineController *controller)
{
  if (controller == NULL)
  {
    return;
  }
  memset(controller, 0, sizeof(*controller));
  controller->last_direction = -1;
  controller->mode = SIMPLE_LINE_STOP;
}

void SimpleLine_Start(SimpleLineController *controller)
{
  if (controller == NULL || controller->mode != SIMPLE_LINE_STOP)
  {
    return;
  }
  SimpleLine_Init(controller);
  controller->mode = SIMPLE_LINE_TRACK;
}

void SimpleLine_Stop(SimpleLineController *controller)
{
  if (controller != NULL)
  {
    set_output(controller, SIMPLE_LINE_STOP, 0, 0);
  }
}

void SimpleLine_StepArc(SimpleLineController *controller, uint8_t raw_mask)
{
  uint8_t mask = raw_mask & 15U;
  SimpleLine_Step(controller, mask);
  if (controller == NULL || controller->mode == SIMPLE_LINE_STOP || mask == 0U)
    return;
  if (mask == 8U || mask == 12U || mask == 4U)
  {
    controller->last_direction = -1;
    set_output(controller, SIMPLE_LINE_TRACK, SIMPLE_LINE_SLOW_PWM, SIMPLE_LINE_OUTER_PWM);
  }
  else if (mask == 1U || mask == 3U || mask == 2U)
  {
    controller->last_direction = 1;
    set_output(controller, SIMPLE_LINE_TRACK, SIMPLE_LINE_OUTER_PWM, SIMPLE_LINE_SLOW_PWM);
  }
  else
    set_output(controller, SIMPLE_LINE_TRACK, SIMPLE_LINE_SLOW_PWM, SIMPLE_LINE_SLOW_PWM);
}

void SimpleLine_StepSlow(SimpleLineController *controller, uint8_t raw_mask)
{
  uint8_t mask = raw_mask & 15U;
  SimpleLine_Step(controller, mask);
  if (controller == NULL || controller->mode == SIMPLE_LINE_STOP || mask == 0U)
    return;
  /* Current contact always releases a stale spin immediately. Gentle middle
     corrections never accelerate the outside wheel above straight speed. */
  if (mask == SIMPLE_LINE_LEFT_INNER || mask == SIMPLE_LINE_RIGHT_INNER)
  {
    int8_t direction = mask == SIMPLE_LINE_LEFT_INNER ? -1 : 1;
    controller->last_direction = direction;
    set_output(controller, SIMPLE_LINE_TRACK,
        direction < 0 ? 2200 : 2300, direction < 0 ? 2300 : 2200);
  }
  else if (mask == 8U || mask == 12U || mask == 1U || mask == 3U)
  {
    int8_t direction = mask & 8U ? -1 : 1;
    controller->last_direction = direction;
    set_output(controller, SIMPLE_LINE_TRACK,
        direction < 0 ? 800 : 2300, direction < 0 ? 2300 : 800);
  }
  else
    set_output(controller, mask == 6U ? SIMPLE_LINE_TRACK : SIMPLE_LINE_WIDE, 2300, 2300);
}

void SimpleLine_UpdateYaw(SimpleLineController *controller, int64_t yaw_mdeg,
                          uint8_t valid, uint32_t generation)
{
  if (!controller) return;
  if (!valid || !controller->yaw_configured || controller->yaw_generation != generation)
    controller->line_yaw_valid = controller->sector_active = controller->curve_yaw_valid = 0U;
  controller->yaw_configured=1U; controller->yaw_valid=valid;
  controller->yaw_generation=generation; controller->yaw_mdeg=yaw_mdeg;
}

void SimpleLine_StepRoute(SimpleLineController *controller, uint8_t raw_mask,
                          const SignRouteStatus *route, const SignRouteCommand *command)
{
  uint8_t tracking_mask = raw_mask & 15U;
  uint8_t entry_guard;
  int64_t lower, upper;
  if (!controller || !route || !command || controller->mode == SIMPLE_LINE_STOP) return;
  entry_guard = controller->yaw_configured && route->yaw_valid && route->direction &&
      !route->entry_line_ready &&
      (route->state == SIGN_ROUTE_PROBE || route->state == SIGN_ROUTE_SELECTING);
  if (entry_guard && !controller->entry_guard_active) controller->sector_active=0U;
  controller->entry_guard_active=entry_guard;
  if (controller->route_state != (uint8_t)route->state)
  {
    controller->route_hint = 0;
    controller->curve_yaw_valid = 0U;
  }
  if (route->state == SIGN_ROUTE_PROBE && route->direction && !controller->route_hint)
  {
    SimpleLine_SetDirection(controller, route->direction);
    controller->route_hint = route->direction;
  }
  else if (route->state != SIGN_ROUTE_PROBE && route->state != SIGN_ROUTE_ARC &&
           command->just_started && route->direction)
    SimpleLine_SetDirection(controller, route->direction);
  controller->route_state = (uint8_t)route->state;
  if (entry_guard)
  {
    uint8_t opposite = route->direction < 0 ?
        (tracking_mask == 1U || tracking_mask == 3U) :
        (tracking_mask == 8U || tracking_mask == 12U);
    /* An opposing outer branch is not capture. A lone inner sensor still
       describes the approach line and must permit normal forward correction,
       including immediately after observation; it is not imaginary white. */
    if (opposite) tracking_mask=0U;
    SimpleLine_SetDirection(controller, route->direction);
  }
  /* Current line wins even on the same cycle as a route transition. Never
     write last_direction after this calculation: that poisons the next loss. */
  SimpleLine_StepSlow(controller, tracking_mask);
  if (!controller->yaw_configured) return; /* legacy standalone callers */
  if (tracking_mask)
  {
    controller->sector_active=0U;
    if (controller->yaw_valid)
    {
      /* A centred sensor pair has no turn direction. On an acquired arc,
         use observed curvature rather than retaining the entry sign forever.
         One-sided line evidence remains authoritative; white/search motion
         must never be learned as the curve's direction. */
      if (route->state == SIGN_ROUTE_ARC)
      {
        int64_t curve_delta=controller->yaw_mdeg-controller->curve_yaw_mdeg;
        if (controller->curve_yaw_valid && tracking_mask == 6U &&
            (curve_delta >= SIMPLE_LINE_ARC_TREND_MDEG ||
             curve_delta <= -SIMPLE_LINE_ARC_TREND_MDEG))
        {
          controller->last_direction=curve_delta > 0 ? -1 : 1;
          controller->curve_yaw_valid=0U;
        }
        if (!controller->curve_yaw_valid || tracking_mask != 6U)
          controller->curve_yaw_mdeg=controller->yaw_mdeg;
        controller->curve_yaw_valid=1U;
      }
      else controller->curve_yaw_valid=0U;
      controller->line_yaw_mdeg=controller->yaw_mdeg;
      controller->line_yaw_valid=1U;
    }
    return;
  }
  controller->curve_yaw_valid=0U;
  if (!controller->yaw_valid)
  {
    /* No trustworthy heading: withdraw search rather than rotate unbounded.
       Keep SEARCH state, so fresh yaw or line can resume without a new START. */
    set_output(controller,SIMPLE_LINE_SEARCH,0,0);
    return;
  }
  if (!controller->line_yaw_valid)
  {
    controller->line_yaw_mdeg=controller->yaw_mdeg;
    controller->line_yaw_valid=1U;
  }
  if (!controller->sector_active)
  {
    controller->sector_active=1U;
    controller->sector_direction=controller->last_direction;
  }
  lower = -SIMPLE_LINE_SEARCH_SECTOR_MDEG;
  upper = SIMPLE_LINE_SEARCH_SECTOR_MDEG;
  if (entry_guard)
  {
    /* Keep a phase-anchored selected-side sector large enough to acquire the
       entry tangent. Repeated contacts must not walk its far bound around
       the circle. Ordinary acquired-arc searches retain the +/-25-degree guard. */
    int64_t phase_origin=controller->yaw_mdeg-route->yaw_mdeg-controller->line_yaw_mdeg;
    if (route->direction < 0)
    { lower=phase_origin; upper=lower+SIMPLE_LINE_ENTRY_SEARCH_SECTOR_MDEG; }
    else
    { upper=phase_origin; lower=upper-SIMPLE_LINE_ENTRY_SEARCH_SECTOR_MDEG; }
  }
  if (controller->yaw_mdeg-controller->line_yaw_mdeg >= upper)
    controller->sector_direction=1; /* positive yaw is left; steer back right */
  else if (controller->yaw_mdeg-controller->line_yaw_mdeg <= lower)
    controller->sector_direction=-1;
  set_turn(controller,controller->sector_direction);
  controller->mode=SIMPLE_LINE_SEARCH;
}

uint8_t SimpleLine_ResolveRouteOutput(const SimpleLineController *controller,
                                    const SignRouteCommand *command, uint8_t paused,
                                    int16_t *left, int16_t *right)
{
  *left = *right = 0;
  if (controller->mode == SIMPLE_LINE_STOP) return SIMPLE_LINE_STOP;
  if (paused) return 6U;
  if (command->active)
  {
    *left=command->left_pwm; *right=command->right_pwm;
    return 5U;
  }
  *left=controller->left_pwm; *right=controller->right_pwm;
  return (uint8_t)controller->mode;
}

void SimpleLine_SetDirection(SimpleLineController *controller,
                             int8_t direction)
{
  if (controller != NULL && direction != 0)
  {
    controller->last_direction = direction < 0 ? -1 : 1;
  }
}

void SimpleLine_Step(SimpleLineController *controller, uint8_t raw_mask)
{
  uint8_t value;
  int8_t direction;

  if (controller == NULL)
  {
    return;
  }
  controller->raw_mask = raw_mask & 0x0FU;
  if (controller->ready == 0U)
  {
    controller->candidate_mask = controller->raw_mask;
    controller->filtered_mask = controller->raw_mask;
    controller->sample_count = 1U;
    controller->ready = 1U;
  }
  else if (controller->raw_mask != controller->candidate_mask)
  {
    controller->candidate_mask = controller->raw_mask;
    controller->sample_count = 1U;
  }
  else if (controller->sample_count < SIMPLE_LINE_FILTER_SAMPLES)
  {
    ++controller->sample_count;
  }
  if (controller->sample_count >= SIMPLE_LINE_FILTER_SAMPLES)
  {
    controller->filtered_mask = controller->candidate_mask;
  }
  if (controller->mode == SIMPLE_LINE_STOP)
  {
    return;
  }

  /* Do not replay a positive forward target after the raw sensors lose the
     line. Two-sample filtering still applies to reacquisition/other patterns. */
  value = controller->raw_mask == 0U ? 0U : controller->filtered_mask;
  if (value == (SIMPLE_LINE_LEFT_INNER | SIMPLE_LINE_RIGHT_INNER))
  {
    set_output(controller, SIMPLE_LINE_TRACK,
               SIMPLE_LINE_STRAIGHT_PWM, SIMPLE_LINE_STRAIGHT_PWM);
  }
  else if (value == SIMPLE_LINE_LEFT_INNER ||
           value == SIMPLE_LINE_RIGHT_INNER)
  {
    direction = value == SIMPLE_LINE_LEFT_INNER ? -1 : 1;
    controller->last_direction = direction;
    set_output(controller, SIMPLE_LINE_TRACK,
               direction < 0 ? SIMPLE_LINE_SLOW_PWM : SIMPLE_LINE_OUTER_PWM,
               direction < 0 ? SIMPLE_LINE_OUTER_PWM : SIMPLE_LINE_SLOW_PWM);
  }
  else if (value == SIMPLE_LINE_LEFT_OUTER ||
           value == (SIMPLE_LINE_LEFT_OUTER | SIMPLE_LINE_LEFT_INNER) ||
           value == SIMPLE_LINE_RIGHT_OUTER ||
           value == (SIMPLE_LINE_RIGHT_OUTER | SIMPLE_LINE_RIGHT_INNER))
  {
    direction = (value & SIMPLE_LINE_LEFT_OUTER) != 0U ? -1 : 1;
    controller->last_direction = direction;
    set_turn(controller, direction);
  }
  else if (value == 0U)
  {
    set_turn(controller, controller->last_direction);
    controller->mode = SIMPLE_LINE_SEARCH;
  }
  else
  {
    set_output(controller, SIMPLE_LINE_WIDE,
               SIMPLE_LINE_SLOW_PWM, SIMPLE_LINE_SLOW_PWM);
  }
}
