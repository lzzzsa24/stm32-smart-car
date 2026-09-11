/*
 * K210 视觉任务帧接收（新增代码）
 *
 * 接收 K210 通过 USART2(PD5/PD6, 115200) 发来的任务帧：
 *     $T,<mode>,<marker>,<cx>,<cy>,<area>#
 *   mode   : 0 无 / 1 任务派发 / 2 救人 / 3 送物资
 *   marker : -1 无 / 1 救人(橙色圆) / 2 送物资(紫色圆) / 3 人 /
 *            4 物资点(绿) / 5 危险区(红) / 6 余震预警(黄) / 7 复位牌(蓝)
 *   cx,cy  : 目标中心坐标（-1 表示无）
 *   area   : 目标面积（-1 表示无）
 *
 * 与实验七 vision_uart.c 的区别：这里只解析 $T 帧，最新一帧覆盖旧帧。
 */

#ifndef VISION_TASK_H
#define VISION_TASK_H

#include <stdint.h>

typedef struct
{
  int8_t  mode;        /* 0..3 */
  int8_t  marker;      /* -1..7 */
  int16_t cx;          /* -1..319 */
  int16_t cy;          /* -1..239 */
  int32_t area;        /* -1.. */
  uint32_t received_ms;/* STM32 收帧时刻 */
  uint32_t sequence;   /* 每帧有效帧递增 */
} QuakeVisionFrame;

void vision_task_init(void);
/* 主循环反复调用，从环形缓冲消费字节并解析帧。 */
void vision_task_poll(void);
/* 在 USART2_IRQHandler 中调用（替换原 vision_uart_irq_handler）。 */
void vision_task_irq_handler(void);
/* 取出最新一帧；返回 1 表示有新帧，0 表示没有。 */
uint8_t vision_task_take_frame(QuakeVisionFrame *frame);
uint32_t vision_task_bad_frames(void);

#endif /* VISION_TASK_H */
