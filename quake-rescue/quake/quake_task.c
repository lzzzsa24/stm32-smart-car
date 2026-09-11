/*
 * 地震灾后救援任务状态机（新增代码）
 */

#include "quake_task.h"

static QuakeState state;
static QuakeTaskType task_type;
static QuakeAlertType alert_type;
static uint32_t action_until_ms;
static QuakeAudioEvent audio_event;
static uint32_t danger_vacuum_until;   /* 危险/预警处理完后的不识别真空期 */
static uint32_t action_vacuum_until;   /* 人员/物资处理完后的不识别真空期 */
static uint32_t task_vacuum_until;     /* 任务牌派发后忽略任务牌/复位牌的真空截止时刻 */
static uint8_t red_danger_flag;        /* 识别到红色危险（非黄）的一次性标志 */

/* 「识别到第一帧就停车、若干帧后确认」的确认状态。 */
static int8_t confirm_marker;          /* 正在确认的候选标记 */
static uint8_t confirm_frames_seen;    /* 已观察帧数（含第一帧，窗口计数） */
static uint8_t confirm_frames_hit;     /* 观察期间命中（与 confirm_marker 相同）的帧数 */
static uint32_t confirm_until_ms;      /* 确认最长等待截止时刻（兜底超时） */
static QuakeState confirm_return_state; /* CONFIRMING 超时/放弃后回到的状态（PATROL/WAIT_TASK） */

void QuakeTask_Init(void)
{
  state = QUAKE_STATE_IDLE;
  task_type = QUAKE_TASK_NONE;
  alert_type = QUAKE_ALERT_NONE;
  action_until_ms = 0U;
  audio_event = QUAKE_AUDIO_NONE;
  danger_vacuum_until = 0U;
  action_vacuum_until = 0U;
  task_vacuum_until = 0U;
  red_danger_flag = 0U;
  confirm_marker = QUAKE_MARKER_NONE;
  confirm_frames_seen = 0U;
  confirm_frames_hit = 0U;
  confirm_until_ms = 0U;
  confirm_return_state = QUAKE_STATE_PATROL;
}

void QuakeTask_Start(void)
{
  if (state == QUAKE_STATE_IDLE || state == QUAKE_STATE_STOPPED)
  {
    /* 按 KEY1 只进入「等待派发任务」，不动；收到蓝/橙任务牌后才进入巡逻开动。 */
    state = QUAKE_STATE_WAIT_TASK;
    task_type = QUAKE_TASK_NONE;
    alert_type = QUAKE_ALERT_NONE;
    action_until_ms = 0U;
    audio_event = QUAKE_AUDIO_START;
    danger_vacuum_until = 0U;
    action_vacuum_until = 0U;
  task_vacuum_until = 0U;
    red_danger_flag = 0U;
    confirm_marker = QUAKE_MARKER_NONE;
    confirm_frames_seen = 0U;
    confirm_frames_hit = 0U;
    confirm_until_ms = 0U;
    confirm_return_state = QUAKE_STATE_PATROL;
  }
}

void QuakeTask_Stop(void)
{
  state = QUAKE_STATE_STOPPED;
  task_type = QUAKE_TASK_NONE;
  alert_type = QUAKE_ALERT_NONE;
  action_until_ms = 0U;
  audio_event = QUAKE_AUDIO_NONE;
  danger_vacuum_until = 0U;
  action_vacuum_until = 0U;
  task_vacuum_until = 0U;
  red_danger_flag = 0U;
  confirm_marker = QUAKE_MARKER_NONE;
  confirm_frames_seen = 0U;
  confirm_frames_hit = 0U;
  confirm_until_ms = 0U;
  confirm_return_state = QUAKE_STATE_PATROL;
}

static void enter_action(uint32_t now_ms)
{
  state = QUAKE_STATE_ACTION;
  action_until_ms = now_ms + QUAKE_ACTION_MAX_MS;
}

/* 巡逻时，这个标记是否值得「停车确认」（考虑任务门控 + 7s 真空期）。 */
static uint8_t patrol_marker_relevant(int8_t marker, uint32_t now_ms)
{
  switch (marker)
  {
    /* 人（PERSON）已改在 OnFrame 的 PATROL 分支里直接处理：识别到立即停车播语音，
       不走这里的多帧确认。 */
    case QUAKE_MARKER_SUPPLY:
      return (task_type == QUAKE_TASK_DELIVER &&
              (int32_t)(now_ms - action_vacuum_until) >= 0) ? 1U : 0U;
    case QUAKE_MARKER_DANGER:
    case QUAKE_MARKER_WARNING:
      return ((int32_t)(now_ms - danger_vacuum_until) >= 0) ? 1U : 0U;
    /* 任务牌（橙/紫）只在 WAIT_TASK 状态里派发，巡逻态不再处理。 */
    default:
      return 0U;
  }
}

/* 确认完成后，进入对应的识别结果。 */
static void patrol_dispatch(int8_t marker, uint32_t now_ms)
{
  if (marker == QUAKE_MARKER_TASK_RESCUE)
  {
    /* 确认到橙色任务牌：接救人任务，开始巡逻。 */
    task_type = QUAKE_TASK_RESCUE;
    state = QUAKE_STATE_PATROL;
    alert_type = QUAKE_ALERT_NONE;
    audio_event = QUAKE_AUDIO_TASK_RESCUE;
    task_vacuum_until = now_ms + QUAKE_TASK_VACUUM_MS;
  }
  else if (marker == QUAKE_MARKER_TASK_DELIVER)
  {
    /* 确认到紫色任务牌：接送物资任务，开始巡逻。 */
    task_type = QUAKE_TASK_DELIVER;
    state = QUAKE_STATE_PATROL;
    alert_type = QUAKE_ALERT_NONE;
    audio_event = QUAKE_AUDIO_TASK_DELIVER;
    task_vacuum_until = now_ms + QUAKE_TASK_VACUUM_MS;
  }
  else if (marker == QUAKE_MARKER_PERSON)
  {
    /* 救人途中识别到人：停车救人（蓝光闪烁） */
    enter_action(now_ms);
    alert_type = QUAKE_ALERT_PERSON;
    audio_event = QUAKE_AUDIO_RESCUE;
  }
  else if (marker == QUAKE_MARKER_SUPPLY)
  {
    /* 送物资途中到达物资点：停车投放（绿光闪烁） */
    enter_action(now_ms);
    alert_type = QUAKE_ALERT_SUPPLY;
    audio_event = QUAKE_AUDIO_DELIVER;
  }
  else if (marker == QUAKE_MARKER_DANGER || marker == QUAKE_MARKER_WARNING)
  {
    /* 危险区 / 余震预警：停车避险，并记录报警类型（红/黄灯） */
    state = QUAKE_STATE_ALERT;
    alert_type = (marker == QUAKE_MARKER_DANGER) ? QUAKE_ALERT_DANGER
                                                 : QUAKE_ALERT_WARNING;
    audio_event = QUAKE_AUDIO_DANGER;
    if (marker == QUAKE_MARKER_DANGER)
    {
      red_danger_flag = 1U;   /* 只有红色危险才开启避障（黄不开启） */
    }
  }
  /* 复位牌（蓝）已在 OnFrame 的 PATROL 分支里直接处理：静默清任务、回等待派发。 */
}

void QuakeTask_OnFrame(const QuakeVisionFrame *frame, uint32_t now_ms)
{
  int8_t marker;

  if (frame == 0)
  {
    return;
  }
  marker = frame->marker;

  switch (state)
  {
    case QUAKE_STATE_PATROL:
      /* 复位牌（蓝）：静默切回「等待派发」（停车），不播报、不进入确认。
         任务牌刚派发后的真空期内忽略，避免重复识别。 */
      if (marker == QUAKE_MARKER_RESET &&
          (int32_t)(now_ms - task_vacuum_until) >= 0)
      {
        task_type = QUAKE_TASK_NONE;
        alert_type = QUAKE_ALERT_NONE;
        state = QUAKE_STATE_WAIT_TASK;
        break;
      }
      /* 识别到人：立即停车播语音，不做多帧确认（救人任务内、且不在真空期）。 */
      if (marker == QUAKE_MARKER_PERSON &&
          task_type == QUAKE_TASK_RESCUE &&
          (int32_t)(now_ms - action_vacuum_until) >= 0)
      {
        patrol_dispatch(QUAKE_MARKER_PERSON, now_ms);
        break;
      }
      /* 其余标记：识别到第一帧就停车，进入确认（见 CONFIRMING）。 */
      if (marker != QUAKE_MARKER_NONE &&
          patrol_marker_relevant(marker, now_ms) != 0U)
      {
        state = QUAKE_STATE_CONFIRMING;
        confirm_marker = marker;
        confirm_frames_seen = 1U;
        confirm_frames_hit = 1U;
        confirm_until_ms = now_ms + QUAKE_CONFIRM_TIMEOUT_MS;
        confirm_return_state = QUAKE_STATE_PATROL;
      }
      break;

    case QUAKE_STATE_CONFIRMING:
      /* 停车观察：在窗口帧数内命中足够帧就确认，超过窗口就放弃恢复巡逻。
         允许中间有几帧没识别到（K210 帧率低/检测抖动时更抗丢帧）。 */
      if (confirm_frames_seen < 255U)
      {
        ++confirm_frames_seen;
      }
      if (marker == confirm_marker)
      {
        if (confirm_frames_hit < 255U)
        {
          ++confirm_frames_hit;
        }
      }
      if (confirm_frames_hit >= QUAKE_MARKER_CONFIRM_FRAMES)
      {
        patrol_dispatch(confirm_marker, now_ms);
      }
      else if (confirm_frames_seen >= QUAKE_MARKER_CONFIRM_WINDOW_FRAMES)
      {
        state = confirm_return_state;
      }
      break;

    case QUAKE_STATE_WAIT_TASK:
      /* 停车等待派发任务：橙/紫任务牌走多帧确认；派发后的真空期内忽略。 */
      if ((marker == QUAKE_MARKER_TASK_RESCUE ||
           marker == QUAKE_MARKER_TASK_DELIVER) &&
          (int32_t)(now_ms - task_vacuum_until) >= 0)
      {
        state = QUAKE_STATE_CONFIRMING;
        confirm_marker = marker;
        confirm_frames_seen = 1U;
        confirm_frames_hit = 1U;
        confirm_until_ms = now_ms + QUAKE_CONFIRM_TIMEOUT_MS;
        confirm_return_state = QUAKE_STATE_WAIT_TASK;
      }
      break;

    case QUAKE_STATE_ALERT:
      /* 避险中：保持停车，危险语音播完后由 QuakeTask_Task 回到巡逻 */
      break;

    case QUAKE_STATE_ACTION:
    case QUAKE_STATE_IDLE:
    case QUAKE_STATE_STOPPED:
    default:
      break;
  }
}

void QuakeTask_Task(uint32_t now_ms, uint8_t audio_done)
{
  /* 停车确认的兜底超时：若 K210 中途不再上报，超过时限就恢复巡逻继续走，
     避免一直停在原地。 */
  if (state == QUAKE_STATE_CONFIRMING &&
      (int32_t)(now_ms - confirm_until_ms) >= 0)
  {
    state = confirm_return_state;
  }

  /* 动作（救人/投放）：相关语音播完后再出发；若语音异常一直不报「播完」，
     用 action_until_ms 兜底，避免永远卡在停车。 */
  if (state == QUAKE_STATE_ACTION &&
      (audio_done != 0U || (int32_t)(now_ms - action_until_ms) >= 0))
  {
    /* 动作完成：回巡逻（继续走动），清任务；进入 7s 不识别真空期，
       防止同一个目标被反复触发。 */
    state = QUAKE_STATE_PATROL;
    task_type = QUAKE_TASK_NONE;
    alert_type = QUAKE_ALERT_NONE;
    action_vacuum_until = now_ms + QUAKE_VACUUM_MS;
  }

  /* 避险（危险/预警）：危险语音播完就回巡逻，并进入 7s 不识别真空期，
     让小车能继续往前开（不被同一张危险牌一直挡停）。 */
  if (state == QUAKE_STATE_ALERT && audio_done != 0U)
  {
    state = QUAKE_STATE_PATROL;
    alert_type = QUAKE_ALERT_NONE;
    danger_vacuum_until = now_ms + QUAKE_VACUUM_MS;
  }
}

QuakeState QuakeTask_GetState(void)
{
  return state;
}

QuakeTaskType QuakeTask_GetTaskType(void)
{
  return task_type;
}

QuakeAlertType QuakeTask_GetAlertType(void)
{
  return alert_type;
}

uint8_t QuakeTask_IsMotionLocked(void)
{
  return (state == QUAKE_STATE_IDLE || state == QUAKE_STATE_ACTION ||
          state == QUAKE_STATE_ALERT || state == QUAKE_STATE_WAIT_TASK ||
          state == QUAKE_STATE_STOPPED || state == QUAKE_STATE_CONFIRMING);
}

uint8_t QuakeTask_IsAlerting(void)
{
  return (state == QUAKE_STATE_ACTION || state == QUAKE_STATE_ALERT);
}

QuakeAudioEvent QuakeTask_TakeAudioEvent(void)
{
  QuakeAudioEvent event = audio_event;

  audio_event = QUAKE_AUDIO_NONE;
  return event;
}

uint8_t QuakeTask_TakeRedDanger(void)
{
  uint8_t flag = red_danger_flag;

  red_danger_flag = 0U;
  return flag;
}
