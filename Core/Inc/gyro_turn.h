#ifndef GYRO_TURN_H
#define GYRO_TURN_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* API v1: single shared, cooperative in-place yaw controller. Main-loop only.
   The mode arbiter must grant exclusive DriveBase ownership before Start,
   keep line/position/automatic-recovery writers suspended while RUNNING,
   and call Stop before handing motors to another mode. No internal mutex.
   Global sensor service: MpuYaw_Task once per main-loop iteration, BEFORE
   GyroTurn_Task. Start/Task also refresh that same service at point of use.
   Do not reinitialize/calibrate the sensor during a turn. */
typedef enum { GYRO_TURN_IDLE, GYRO_TURN_RUNNING,
               GYRO_TURN_DONE, GYRO_TURN_FAULT } GyroTurnState;
enum { GYRO_TURN_SENSOR = 1, GYRO_TURN_DRIVE, GYRO_TURN_TIMEOUT,
       GYRO_TURN_DIRECTION, GYRO_TURN_NO_PROGRESS, GYRO_TURN_ACCURACY,
       GYRO_TURN_DATA_GAP };
/* One-shot relative angle from current yaw: +90000 = left 90 degrees,
   -90000 = right 90 degrees. Wheel CPS, NOT PWM. Nonzero +/-360000 mdeg;
   1412..3600 CPS. Requires stopped, fault-free DriveBase and fresh READY IMU.
   Returns 1 if started (motor command issued immediately), 0 if rejected.
   Rejection need not set FAULT: caller must check the return value.
   Call only on an explicit action event, never repeatedly each loop. */
uint8_t GyroTurn_Start(int32_t angle_mdeg, int32_t maximum_cps);
/* Active owner calls every main-loop iteration; never wait in a while loop.
   RUNNING includes braking/settling. DONE/FAULT persist until Stop, accepted
   next Start or explicit fault clear. Task never auto-starts an action. */
void GyroTurn_Task(void);
/* Application boundary capture: brake, settle, then DONE even before the
   target angle. This is NOT operator STOP and may advance a mode sequence. */
uint8_t GyroTurn_RequestStop(void);
/* Operator cancellation / mode exit: cancel RUNNING and coast, enter IDLE,
   retain fault and last angle. Global STOP also stops other motor owners. */
void GyroTurn_Stop(void);
/* Explicit recovery only, requires stopped DriveBase and non-running turn.
   Does not repair IMU faults, clear DriveBase faults or recalibrate yaw. */
uint8_t GyroTurn_ClearFault(void);
uint8_t GyroTurn_GetFault(void);
/* Clear only an aborted transient data-gap action, when STOPPED and fresh.
   Does not start/resume motion, reset yaw, or clear any hard fault. */
uint8_t GyroTurn_ClearTransientFault(void);
GyroTurnState GyroTurn_GetState(void);
/* Last serviced signed relative yaw, including settling. Capture on DONE
   before starting another action. Preserved on Stop; reset on accepted Start. */
int32_t GyroTurn_GetAchievedAngleMdeg(void);
#ifdef __cplusplus
}
#endif
#endif
