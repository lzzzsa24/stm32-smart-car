#ifndef LINE_BYPASS_TRAVEL_H
#define LINE_BYPASS_TRAVEL_H
#include <stdint.h>

typedef enum {
  LINE_BYPASS_TRAVEL_IDLE = 0,
  LINE_BYPASS_TRAVEL_RUNNING,
  LINE_BYPASS_TRAVEL_DONE,
  LINE_BYPASS_TRAVEL_FAULT
} LineBypassTravelState;

/* KEY1 short travel only. Positive mm is forward. All wheels run in speed
   mode at up to 1800 CPS, then brake together; this is not exact per-wheel
   endpoint control. The existing speed ramp/PI remains active throughout. */
uint8_t LineBypassTravel_Start(int32_t distance_mm, int32_t cps);
void LineBypassTravel_Task(void);
void LineBypassTravel_Stop(void);
LineBypassTravelState LineBypassTravel_GetState(void);
uint8_t LineBypassTravel_GetFaultMask(void);
uint32_t LineBypassTravel_GetProgressMm(void);
#endif
