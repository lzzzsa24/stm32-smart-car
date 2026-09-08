#ifndef GYRO_TURN_H
#define GYRO_TURN_H
#include <stdint.h>
typedef enum { GYRO_TURN_IDLE, GYRO_TURN_RUNNING,
               GYRO_TURN_DONE, GYRO_TURN_FAULT } GyroTurnState;
enum { GYRO_TURN_SENSOR = 1, GYRO_TURN_DRIVE, GYRO_TURN_TIMEOUT,
       GYRO_TURN_DIRECTION, GYRO_TURN_NO_PROGRESS, GYRO_TURN_ACCURACY };
uint8_t GyroTurn_Start(int32_t angle_mdeg, int32_t maximum_cps);
void GyroTurn_Task(void);
uint8_t GyroTurn_RequestStop(void); /* IR boundary: early success after settling */
void GyroTurn_Stop(void); /* cancel, never resume; retains latched fault */
uint8_t GyroTurn_ClearFault(void); /* requires stopped DriveBase */
uint8_t GyroTurn_GetFault(void);
GyroTurnState GyroTurn_GetState(void);
int32_t GyroTurn_GetAchievedAngleMdeg(void);
#endif
