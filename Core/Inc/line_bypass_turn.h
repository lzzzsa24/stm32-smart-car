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

/* KEY1 only: positive angle is left. Default MPU6050 yaw endpoint and
   encoder wheel-speed control. MPU6050_BYPASS_ENABLED=0 selects legacy
   encoder endpoints at build time, never as an automatic sensor fallback. */
uint8_t LineBypassTurn_Start(int32_t angle_mdeg, int32_t cps);
void LineBypassTurn_Task(void);
uint8_t LineBypassTurn_RequestStop(void);
void LineBypassTurn_Stop(void);
LineBypassTurnState LineBypassTurn_GetState(void);
uint8_t LineBypassTurn_GetFaultMask(void);
int32_t LineBypassTurn_GetAchievedAngleMdeg(void);

#endif
