#ifndef SIMPLE_LINE_MODE_H
#define SIMPLE_LINE_MODE_H

#include <stdint.h>

#define SIMPLE_LINE_LEFT_OUTER  8U
#define SIMPLE_LINE_LEFT_INNER  4U
#define SIMPLE_LINE_RIGHT_INNER 2U
#define SIMPLE_LINE_RIGHT_OUTER 1U

typedef enum
{
  SIMPLE_LINE_STOP = 0,
  SIMPLE_LINE_TRACK,
  SIMPLE_LINE_TURN,
  SIMPLE_LINE_SEARCH,
  SIMPLE_LINE_WIDE
} SimpleLineMode;

typedef struct
{
  SimpleLineMode mode;
  uint8_t raw_mask;
  uint8_t filtered_mask;
  uint8_t candidate_mask;
  uint8_t sample_count;
  uint8_t ready;
  int8_t last_direction;
  int16_t left_pwm;
  int16_t right_pwm;
} SimpleLineController;

void SimpleLine_Init(SimpleLineController *controller);
void SimpleLine_Start(SimpleLineController *controller);
void SimpleLine_Stop(SimpleLineController *controller);
void SimpleLine_SetDirection(SimpleLineController *controller,
                             int8_t direction);
void SimpleLine_Step(SimpleLineController *controller, uint8_t raw_mask);
/* Arc tracking: visible line uses forward differential drive, white searches. */
void SimpleLine_StepArc(SimpleLineController *controller, uint8_t raw_mask);

#endif
