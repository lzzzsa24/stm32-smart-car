#ifndef VISION_DETECTION_H
#define VISION_DETECTION_H

#include <stdint.h>

typedef struct
{
  int8_t class_id;       /* -1=none, 0=left, 1=right, 2=horn, 3=one, 4=two */
  uint8_t score;         /* 0..100 */
  uint16_t center_x;     /* 0..319 for a detected object */
  uint16_t center_y;     /* 0..239 for a detected object */
  uint32_t received_ms;  /* STM32 receive time, not a K210 timestamp */
  uint32_t sequence;     /* increments for every valid complete frame */
} VisionDetection;

#endif
