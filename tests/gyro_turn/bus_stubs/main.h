#ifndef GYRO_BUS_TEST_MAIN_H
#define GYRO_BUS_TEST_MAIN_H
#include <stdint.h>
typedef int GPIO_TypeDef;
typedef struct { uint32_t Mode, Pull, Speed, Pin; } GPIO_InitTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef struct { uint32_t CYCCNT, CTRL; } TestDwt;
typedef struct { uint32_t DEMCR; } TestDebug;
extern TestDwt test_dwt;
extern TestDebug test_debug;
extern uint32_t SystemCoreClock;
#define DWT (&test_dwt)
#define CoreDebug (&test_debug)
#define CoreDebug_DEMCR_TRCENA_Msk 1U
#define DWT_CTRL_CYCCNTENA_Msk 1U
#define __NOP() ((void)(test_dwt.CYCCNT += 72U))
#define GPIOB ((GPIO_TypeDef *)1)
#define GPIOE ((GPIO_TypeDef *)2)
#define GPIO_PIN_0 1U
#define GPIO_PIN_10 1024U
#define GPIO_PIN_11 2048U
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_MODE_OUTPUT_OD 2U
#define GPIO_NOPULL 0U
#define GPIO_PULLUP 1U
#define GPIO_SPEED_FREQ_LOW 0U
#define GPIO_SPEED_FREQ_HIGH 1U
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOE_CLK_ENABLE() ((void)0)
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *gpio);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
#endif
