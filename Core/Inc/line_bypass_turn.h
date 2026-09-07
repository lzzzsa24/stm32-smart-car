#ifndef LINE_BYPASS_TURN_H
#define LINE_BYPASS_TURN_H

#include <stdint.h>

typedef enum
{
  LINE_BYPASS_TURN_IDLE = 0,
  LINE_BYPASS_TURN_RUNNING,
  LINE_BYPASS_TURN_DONE,
  LINE_BYPASS_TURN_FAULT
} LineBypassTurnState;

/* KEY1 only: positive angle is left. Continuous four-wheel speed control;
   encoder travel bounds a step but does not measure actual chassis yaw. */
uint8_t LineBypassTurn_Start(int32_t angle_mdeg, int32_t cps);
void LineBypassTurn_Task(void);
uint8_t LineBypassTurn_RequestStop(void);
void LineBypassTurn_Stop(void);
LineBypassTurnState LineBypassTurn_GetState(void);
uint8_t LineBypassTurn_GetFaultMask(void);
int32_t LineBypassTurn_GetAchievedAngleMdeg(void);

#endif
