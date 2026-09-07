#ifndef VISION_LINE_V4_CONTROL_H
#define VISION_LINE_V4_CONTROL_H

#include <stdint.h>

#include "vision_line_v4.h"

typedef enum
{
  VISION_LINE_V4_WAITING = 0U,
  VISION_LINE_V4_FOLLOW,
  VISION_LINE_V4_CURVE,
  VISION_LINE_V4_SHARP_TURN,
  VISION_LINE_V4_LOST_HOLD,
  VISION_LINE_V4_OBSTACLE_STOP,
  VISION_LINE_V4_LOST_STOP,
  VISION_LINE_V4_LINK_STOP
} VisionLineV4ControlState;

typedef struct
{
  VisionLineV4ControlState state;
  int16_t left_pwm;
  int16_t right_pwm;
  int16_t filtered_offset;
  int8_t turn_direction;
} VisionLineV4Command;

void VisionLineV4Control_Init(void);
void VisionLineV4Control_Step(const VisionLineV4Reading *reading,
                              uint32_t now_ms,
                              VisionLineV4Command *command);

#endif
