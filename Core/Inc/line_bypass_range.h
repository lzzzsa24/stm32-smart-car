#ifndef LINE_BYPASS_RANGE_H
#define LINE_BYPASS_RANGE_H
#include <stdint.h>

/* Active bypass ranging without ultrasonic motor callbacks. Discard a shot
   across turning/stopping so a previous heading cannot truncate the next leg. */
void LineBypassRange_Reset(void);
uint8_t LineBypassRange_Task(uint8_t forward_active, uint16_t obstacle_cm);
#endif
