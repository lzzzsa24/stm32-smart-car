/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ultrasonic_avoid.h
  * @brief   与电机实现无关的超声波避障状态机
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __ULTRASONIC_AVOID_H
#define __ULTRASONIC_AVOID_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ULTRASONIC_AVOID_TURN_LEFT   0U
#define ULTRASONIC_AVOID_TURN_RIGHT  1U

typedef enum
{
  ULTRASONIC_AVOID_WAIT_SAFE = 0U,
  ULTRASONIC_AVOID_FORWARD,
  ULTRASONIC_AVOID_STOPPING,
  ULTRASONIC_AVOID_BACKING,
  ULTRASONIC_AVOID_GUARD,
  ULTRASONIC_AVOID_TURNING,
  ULTRASONIC_AVOID_COOLDOWN
} UltrasonicAvoidState;

typedef void (*UltrasonicAvoidDriveCallback)(int16_t left_speed,
                                             int16_t right_speed);
typedef void (*UltrasonicAvoidStopCallback)(void);
typedef void (*UltrasonicAvoidTurnCallback)(int16_t inner_speed,
                                            int16_t outer_speed);

void UltrasonicAvoid_Init(UltrasonicAvoidDriveCallback drive_callback,
                          UltrasonicAvoidStopCallback stop_callback,
                          UltrasonicAvoidTurnCallback turn_left_callback,
                          UltrasonicAvoidTurnCallback turn_right_callback);

/* 在主循环中反复调用，不要在其中加入长时间 HAL_Delay。 */
void UltrasonicAvoid_Task(void);

void UltrasonicAvoid_SetThresholds(uint16_t stop_cm, uint16_t clear_cm);
/* Fast path used while approaching at speed.  Two consecutive raw readings
 * at/below this distance trigger immediately without waiting for the
 * three-sample median; a critically close reading triggers at once. */
void UltrasonicAvoid_SetEmergencyDistance(uint16_t emergency_cm);
void UltrasonicAvoid_SetSpeeds(int16_t cruise_speed,
                               int16_t slow_speed,
                               int16_t turn_inner_speed,
                               int16_t turn_outer_speed);
void UltrasonicAvoid_SetTurnTime(uint32_t turn_time_ms);

/* 可选的脱困后退动作。未设置 reverse_callback 时仍可只停车、转向；
   各阶段均由 Task 非阻塞计时。 */
void UltrasonicAvoid_SetEscapeManeuver(
    UltrasonicAvoidDriveCallback reverse_callback,
    int16_t reverse_speed,
    uint32_t stop_time_ms,
    uint32_t reverse_time_ms,
    uint32_t guard_time_ms);

/* 显式启用的低速降级：连续无回波，或700ms无有效结果（含越界/驱动
   卡住）后以slow_speed运行；有效近障回波始终优先处理，随后恢复测距。 */
void UltrasonicAvoid_SetNoEchoFallback(uint8_t enable,
                                       uint8_t timeout_count);

uint8_t UltrasonicAvoid_IsNoEchoFallbackActive(void);

UltrasonicAvoidState UltrasonicAvoid_GetState(void);
uint16_t UltrasonicAvoid_GetLastDistanceCm(void);
uint16_t UltrasonicAvoid_GetEmergencyDistanceCm(void);

#ifdef __cplusplus
}
#endif

#endif /* __ULTRASONIC_AVOID_H */
