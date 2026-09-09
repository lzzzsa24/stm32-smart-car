/* 0.91 inch 128x32 OLED status display on I2C1 PB6/PB7. */

#ifndef __OLED_STATUS_H
#define __OLED_STATUS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void OledStatus_Init(void);
void OledStatus_SetBattery(uint16_t millivolts,
                           uint8_t percent,
                           uint8_t valid,
                           uint8_t low);
void OledStatus_SetData(uint8_t integrated_mode,
                        uint8_t motion_valid,
                        uint16_t distance_mm,
                        int16_t closing_speed_mm_s,
                        int32_t relative_displacement_mm);
void OledStatus_SetFigure8Data(uint8_t state,
                               uint8_t fault_mask,
                               int32_t motor1,
                               int32_t motor2,
                               int32_t motor3,
                               int32_t motor4);
void OledStatus_SetSquareData(uint8_t state,
                              uint8_t side,
                              uint8_t fault_mask,
                              int32_t motor1,
                              int32_t motor2,
                              int32_t motor3,
                              int32_t motor4);
void OledStatus_SetSignLineData(uint8_t line_mask,
                                uint8_t line_action,
                                int8_t vision_class,
                                uint8_t vision_score,
                                uint8_t vision_online,
                                uint8_t route_state,
                                int8_t route_direction);
void OledStatus_SetVisionLineV4Data(uint8_t online,
                                    uint8_t line_found,
                                    int16_t offset,
                                    int16_t angle,
                                    int16_t bottom,
                                    uint8_t obstacle_found,
                                    uint16_t obstacle_bottom,
                                    uint8_t control_state);
void OledStatus_Task(void);
uint8_t OledStatus_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* __OLED_STATUS_H */
