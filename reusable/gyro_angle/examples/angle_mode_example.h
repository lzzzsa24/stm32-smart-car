#ifndef ANGLE_MODE_EXAMPLE_H
#define ANGLE_MODE_EXAMPLE_H
#include <stdint.h>
typedef enum { ANGLE_EXAMPLE_IDLE, ANGLE_EXAMPLE_RUNNING,
               ANGLE_EXAMPLE_DONE, ANGLE_EXAMPLE_FAULT } AngleExampleState;
/* Example only. Arbiter must select this mode and grant motor ownership. */
uint8_t AngleModeExample_Start(int32_t relative_mdeg, int32_t cps);
void AngleModeExample_Task(void); /* after the shared MpuYaw_Task */
void AngleModeExample_Exit(void); /* before handing motors to another owner */
AngleExampleState AngleModeExample_GetState(void);
int32_t AngleModeExample_GetAngleMdeg(void);
#endif
