/*
 * 地震项目用中断服务函数（替换实验七的 stm32f1xx_it.c）
 *
 * 相对实验七的改动（只动了「平台胶水」，没动 motion/ 里的循迹/避障代码）：
 *   1. vision_uart.h 改为 vision_task.h（地震项目用新的 $T 视觉解析）；
 *   2. USART2 中断改调 vision_task_irq_handler()；
 *   3. UART4 中断调 DfPlayerMini_UART4_IRQHandler()（扬声器 DFPlayer 保留）；
 *   4. EXTI15_10 只处理超声波 ECHO，去掉红外遥控 IR_REMOTE。
 *
 * 用法：用它覆盖 CubeIDE 工程 Core/Src/stm32f1xx_it.c。
 */

#include "main.h"
#include "stm32f1xx_it.h"
#include "dfplayer_mini.h"
#include "ultrasonic.h"
#include "vision_task.h"
#include "wheel_encoder.h"

void NMI_Handler(void)
{
  while (1)
  {
  }
}

void HardFault_Handler(void)
{
  while (1)
  {
  }
}

void MemManage_Handler(void)
{
  while (1)
  {
  }
}

void BusFault_Handler(void)
{
  while (1)
  {
  }
}

void UsageFault_Handler(void)
{
  while (1)
  {
  }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
  HAL_IncTick();
  /* HAL_IncTick 已被 motion/line_sensor_clock.c 覆盖，内部会采样四路循迹。 */
}

/* TIM6 常驻 20 kHz，为循迹/绕障/丢线搜索采样四路 AB 相编码器。 */
void TIM6_IRQHandler(void)
{
  WheelEncoder_TIM6_IRQHandler();
}

void EXTI3_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(key1_Pin);
}

void EXTI4_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(key2_Pin);
}

void EXTI9_5_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(key3_Pin);
}

/* PF12/ECHO 使用 EXTI15_10 中断。 */
void EXTI15_10_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(ULTRASONIC_ECHO_Pin);
}

void USART2_IRQHandler(void)
{
  vision_task_irq_handler();
}

/* 扬声器 DFPlayer：UART4 9600 8N1（PC10/TX -> DFPlayer RX，PC11/RX <- DFPlayer TX）。 */
void UART4_IRQHandler(void)
{
  DfPlayerMini_UART4_IRQHandler();
}
