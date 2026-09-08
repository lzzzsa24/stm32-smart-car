#ifndef MPU6050_YAW_H
#define MPU6050_YAW_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Dedicated IMU socket: I2C2 PB10/PB11, AD0 PE0. Component side up,
   Z up: positive yaw is left (CCW viewed from above). */
#ifndef MPU6050_YAW_SIGN
#define MPU6050_YAW_SIGN 1
#endif
#if MPU6050_YAW_SIGN != 1 && MPU6050_YAW_SIGN != -1
#error MPU6050_YAW_SIGN must be 1 or -1
#endif
#ifndef MPU6050_BYPASS_ENABLED
#define MPU6050_BYPASS_ENABLED 1
#endif

typedef enum { MPU_YAW_STARTING, MPU_YAW_CALIBRATING,
               MPU_YAW_READY, MPU_YAW_FAULT } MpuYawState;
enum { MPU_FAULT_BUS = 1, MPU_FAULT_ID, MPU_FAULT_FIFO,
       MPU_FAULT_STALE, MPU_FAULT_CALIBRATION, MPU_FAULT_RANGE };
typedef struct {
  MpuYawState state;
  uint8_t fault;
  uint16_t calibration_samples;
  int32_t bias_milliraw;
  int32_t rate_mdeg_s;
  int64_t yaw_mdeg;
  uint32_t last_sample_ms;
  uint32_t samples;
} MpuYawReading;

/* Sensor service shared by ALL modes; main-loop only, not ISR/reentrant.
   Call only at startup or explicit STOP-state recalibration. No motor IO.
   Resets the shared yaw origin and bias: invalidate old snapshots/targets. */
void MpuYaw_Init(uint32_t now_ms);
/* Call once per main-loop iteration, including STOP and unrelated modes.
   stationary must include STOP ownership and measured wheel standstill.
   FIFO is sensor-timed. Don't place this only in an active turn branch.
   Phase waits are cooperative; bus transactions are bounded, not async. */
void MpuYaw_Task(uint32_t now_ms, uint8_t stationary);
void MpuYaw_GetReading(MpuYawReading *out);
uint8_t MpuYaw_IsReady(uint32_t now_ms);

/* Platform boundary, also used by the host FIFO fault-injection tests. */
uint8_t MpuBus_Init(void);
uint8_t MpuBus_Read(uint8_t reg, uint8_t *data, uint16_t length);
uint8_t MpuBus_Write(uint8_t reg, uint8_t value);
#ifdef __cplusplus
}
#endif
#endif
