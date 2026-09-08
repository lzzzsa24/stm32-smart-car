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

/* KEY1: positive angle is left. Fresh MPU yaw preferred; unavailable IMU or
   ten-second post-failure cooldown selects encoder-estimated endpoints for
   the next action. Never switch angle coordinates during an active turn. */
uint8_t LineBypassTurn_Start(int32_t angle_mdeg, int32_t cps);
void LineBypassTurn_Task(void);
uint8_t LineBypassTurn_RequestStop(void);
void LineBypassTurn_Stop(void);
LineBypassTurnState LineBypassTurn_GetState(void);
uint8_t LineBypassTurn_GetFaultMask(void);
int32_t LineBypassTurn_GetAchievedAngleMdeg(void);
uint8_t LineBypassTurn_UsingGyro(void);
/* Cancelled/stopped owner only: acknowledge angle fault and arm cooldown.
   Does not start motors, reset IMU origin, or clear DriveBase faults. */
void LineBypassTurn_Recover(void);

#endif
