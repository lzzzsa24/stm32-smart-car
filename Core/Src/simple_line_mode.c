#include "simple_line_mode.h"

#include <stddef.h>
#include <string.h>

#define SIMPLE_LINE_FILTER_SAMPLES 2U
#define SIMPLE_LINE_STRAIGHT_PWM 2400
#define SIMPLE_LINE_SLOW_PWM     2200
#define SIMPLE_LINE_OUTER_PWM    2600
#define SIMPLE_LINE_TURN_PWM     2700

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
