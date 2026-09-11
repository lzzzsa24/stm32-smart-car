/*
 * 地震灾后救援任务状态机（新增代码）
 *
 * 只做「决策」，不动电机、不碰硬件；电机由 quake_main.c 依据本模块的
 * 状态去调用 motion/ 里的循迹 + 避障代码。
 *
 * 状态：
 *   IDLE       上电停车，等 KEY1
 *   PATROL     循迹 + 避障巡逻（任务派发 / 救人途中 / 送物资途中）
 *   CONFIRMING 巡逻中识别到标记的第一帧 -> 停车，窗口内命中若干帧后确认再进入对应结果
 *   ACTION     找到目标（人/物资点）→ 停车，播完语音再出发
 *   ALERT      危险区/余震预警 → 停车，播完危险语音再出发
 *   STOPPED    全局停车
 *   WAIT_TASK  等待派发任务（按 KEY1 / 蓝色复位牌后，等橙/紫任务牌）
 */

#ifndef QUAKE_TASK_H
#define QUAKE_TASK_H

#include <stdint.h>

#include "quake_config.h"
#include "vision_task.h"

typedef enum
{
  QUAKE_STATE_IDLE = 0,
  QUAKE_STATE_PATROL,
  QUAKE_STATE_CONFIRMING,
  QUAKE_STATE_ACTION,
  QUAKE_STATE_ALERT,
  QUAKE_STATE_STOPPED,
  QUAKE_STATE_WAIT_TASK
} QuakeState;

typedef enum
{
  QUAKE_TASK_NONE = 0,
  QUAKE_TASK_RESCUE,
  QUAKE_TASK_DELIVER
} QuakeTaskType;

/* 报警类型：危险区（红）与余震预警（黄）用不同颜色灯光区分；
   发现人员（蓝）/到达物资点（绿）也用对应颜色灯光闪烁指示。 */
typedef enum
{
  QUAKE_ALERT_NONE = 0,
  QUAKE_ALERT_DANGER,   /* 危险区（红） */
  QUAKE_ALERT_WARNING,  /* 余震预警（黄） */
  QUAKE_ALERT_PERSON,   /* 发现被困人员（蓝） */
  QUAKE_ALERT_SUPPLY    /* 到达物资点（绿） */
} QuakeAlertType;

/* 状态机产生的「音频事件」，由 quake_main.c 消费后映射到 DFPlayer 曲目。 */
typedef enum
{
  QUAKE_AUDIO_NONE = 0,
  QUAKE_AUDIO_START,        /* 按 KEY1，开始巡逻 */
  QUAKE_AUDIO_TASK_RESCUE,  /* 接到橙色圆，救人任务 */
  QUAKE_AUDIO_TASK_DELIVER, /* 接到紫色圆，送物资任务 */
  QUAKE_AUDIO_RESCUE,       /* 识别人，开始救援 */
  QUAKE_AUDIO_DELIVER,      /* 到达物资点，投放 */
  QUAKE_AUDIO_DANGER,       /* 进入危险/预警/翻车 */
  QUAKE_AUDIO_RESET         /* 复位牌，回任务派发 */
} QuakeAudioEvent;

void QuakeTask_Init(void);
void QuakeTask_Start(void);                         /* KEY1：进入巡逻 */
void QuakeTask_Stop(void);                          /* KEY2：全局停车 */
void QuakeTask_OnFrame(const QuakeVisionFrame *frame, uint32_t now_ms);
/* 每个主循环调用，处理计时；audio_done = 扬声器一次性语音已全部播完。 */
void QuakeTask_Task(uint32_t now_ms, uint8_t audio_done);

QuakeState QuakeTask_GetState(void);
QuakeTaskType QuakeTask_GetTaskType(void);
QuakeAlertType QuakeTask_GetAlertType(void);  /* 当前报警类型（无报警 = NONE） */
uint8_t QuakeTask_IsMotionLocked(void);             /* 1 = 主循环停车，不循迹 */
uint8_t QuakeTask_IsAlerting(void);                 /* 1 = 蜂鸣（动作/避险） */
QuakeAudioEvent QuakeTask_TakeAudioEvent(void);     /* 取走并清除待播音频事件 */
uint8_t QuakeTask_TakeRedDanger(void);              /* 取走「识别到红色危险(非黄)」一次性标志 */

#endif /* QUAKE_TASK_H */
