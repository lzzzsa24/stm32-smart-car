#include "vision_line_v4_control.h"

#include <stddef.h>

#define V4_FRAME_TIMEOUT_MS             150U
#define V4_LOST_HOLD_MS                 400U
#define V4_OBSTACLE_NEAR_BOTTOM         150U
#define V4_OFFSET_DEADBAND                4
#define V4_OFFSET_SLEW_PER_FRAME         80
#define V4_STRAIGHT_PWM                2700
#define V4_CURVE_PWM                   2200
#define V4_STEER_PWM_PER_PIXEL           10
#define V4_MAX_STEER_PWM               1500
#define V4_SHARP_TURN_PWM              2400
#define V4_LOST_HOLD_PWM               2200
#define V4_CURVE_BEND_DEGREES            20
#define V4_SHARP_BEND_DEGREES            70

static uint32_t last_sequence;
static uint32_t lost_since_ms;
static int16_t filtered_offset;
static int8_t last_turn_direction;
static uint8_t line_was_found;
static uint8_t offset_initialized;

static int16_t clamp_forward_pwm(int32_t value)
{
  if (value < 0L)
  {
    return 0;
  }
  if (value > 3400L)
  {
    return 3400;
  }
  return (int16_t)value;
}

static int32_t absolute_i32(int32_t value)
{
  return value < 0L ? -value : value;
}

static void set_spin(VisionLineV4Command *command,
                     int8_t direction,
                     int16_t pwm)
{
  command->turn_direction = direction;
  if (direction < 0)
  {
    command->left_pwm = (int16_t)-pwm;
    command->right_pwm = pwm;
  }
  else
  {
    command->left_pwm = pwm;
    command->right_pwm = (int16_t)-pwm;
  }
}

void VisionLineV4Control_Init(void)
{
  last_sequence = 0U;
  lost_since_ms = 0U;
  filtered_offset = 0;
  last_turn_direction = 0;
  line_was_found = 0U;
  offset_initialized = 0U;
}

void VisionLineV4Control_Step(const VisionLineV4Reading *reading,
                              uint32_t now_ms,
                              VisionLineV4Command *command)
{
  int32_t measured_offset;
  int32_t offset_delta;
  int32_t bend;
  int32_t steer;
  int16_t base_pwm;
  uint8_t new_frame;

  if (command == NULL)
  {
    return;
  }
  command->state = VISION_LINE_V4_WAITING;
  command->left_pwm = 0;
  command->right_pwm = 0;
  command->filtered_offset = filtered_offset;
  command->turn_direction = last_turn_direction;

  if (reading == NULL || reading->frame_valid == 0U)
  {
    return;
  }
  if ((uint32_t)(now_ms - reading->received_ms) > V4_FRAME_TIMEOUT_MS)
  {
    command->state = VISION_LINE_V4_LINK_STOP;
    return;
  }

  new_frame = reading->sequence != last_sequence ? 1U : 0U;
  if (new_frame != 0U)
  {
    last_sequence = reading->sequence;
  }

  if (reading->obstacle_found != 0U &&
      reading->obstacle_bottom >= V4_OBSTACLE_NEAR_BOTTOM)
  {
    command->state = VISION_LINE_V4_OBSTACLE_STOP;
    return;
  }

  if (reading->line_found == 0U)
  {
    if (line_was_found != 0U)
    {
      lost_since_ms = now_ms;
      line_was_found = 0U;
    }
    if (last_turn_direction != 0 && lost_since_ms != 0U &&
        (uint32_t)(now_ms - lost_since_ms) <= V4_LOST_HOLD_MS)
    {
      command->state = VISION_LINE_V4_LOST_HOLD;
      set_spin(command, last_turn_direction, V4_LOST_HOLD_PWM);
      return;
    }
    command->state = VISION_LINE_V4_LOST_STOP;
    return;
  }

  line_was_found = 1U;
  lost_since_ms = 0U;
  if (new_frame != 0U)
  {
    measured_offset = reading->offset;
    if (offset_initialized == 0U)
    {
      filtered_offset = (int16_t)measured_offset;
      offset_initialized = 1U;
    }
    else
    {
      offset_delta = measured_offset - filtered_offset;
      if (offset_delta > V4_OFFSET_SLEW_PER_FRAME)
      {
        measured_offset = filtered_offset + V4_OFFSET_SLEW_PER_FRAME;
      }
      else if (offset_delta < -V4_OFFSET_SLEW_PER_FRAME)
      {
        measured_offset = filtered_offset - V4_OFFSET_SLEW_PER_FRAME;
      }
      filtered_offset = (int16_t)((filtered_offset + measured_offset) / 2L);
    }
  }
  if (filtered_offset > -V4_OFFSET_DEADBAND &&
      filtered_offset < V4_OFFSET_DEADBAND)
  {
    filtered_offset = 0;
  }
  command->filtered_offset = filtered_offset;

  bend = absolute_i32((int32_t)reading->angle - 90L);
  if (bend >= V4_SHARP_BEND_DEGREES)
  {
    int8_t direction;

    if (reading->bottom < 160)
    {
      direction = -1;
    }
    else if (reading->bottom > 160)
    {
      direction = 1;
    }
    else if (filtered_offset < 0)
    {
      direction = -1;
    }
    else if (filtered_offset > 0)
    {
      direction = 1;
    }
    else
    {
      direction = last_turn_direction;
    }
    if (direction == 0)
    {
      command->state = VISION_LINE_V4_LOST_STOP;
      return;
    }
    last_turn_direction = direction;
    command->state = VISION_LINE_V4_SHARP_TURN;
    set_spin(command, direction, V4_SHARP_TURN_PWM);
    return;
  }

  if (filtered_offset < -V4_OFFSET_DEADBAND)
  {
    last_turn_direction = -1;
  }
  else if (filtered_offset > V4_OFFSET_DEADBAND)
  {
    last_turn_direction = 1;
  }
  command->turn_direction = last_turn_direction;
  command->state = bend >= V4_CURVE_BEND_DEGREES ?
                   VISION_LINE_V4_CURVE : VISION_LINE_V4_FOLLOW;
  base_pwm = command->state == VISION_LINE_V4_CURVE ?
             V4_CURVE_PWM : V4_STRAIGHT_PWM;
  steer = (int32_t)filtered_offset * V4_STEER_PWM_PER_PIXEL;
  if (steer > V4_MAX_STEER_PWM)
  {
    steer = V4_MAX_STEER_PWM;
  }
  else if (steer < -V4_MAX_STEER_PWM)
  {
    steer = -V4_MAX_STEER_PWM;
  }
  command->left_pwm = clamp_forward_pwm((int32_t)base_pwm + steer);
  command->right_pwm = clamp_forward_pwm((int32_t)base_pwm - steer);
}
