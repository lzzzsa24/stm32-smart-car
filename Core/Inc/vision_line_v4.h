#ifndef VISION_LINE_V4_H
#define VISION_LINE_V4_H

#include <stdint.h>

typedef struct
{
  uint8_t frame_valid;
  uint8_t line_found;
  int16_t offset;
  int16_t angle;
  int16_t bottom;
  uint8_t obstacle_found;
  uint16_t obstacle_bottom;
  uint16_t obstacle_left;
  uint16_t obstacle_right;
  uint32_t received_ms;
  uint32_t sequence;
} VisionLineV4Reading;

#endif
