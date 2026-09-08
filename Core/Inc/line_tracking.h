/*
 * 实验七：四路红外循迹接口
 *
 * X1～X4 的电平由指导书定义：低电平表示探头位于黑线上，高电平表示
 * 探头位于白底。这里将 X1/X3 作为中间两路，X2/X4 作为左右外侧路。
 */

#ifndef __LINE_TRACKING_H
#define __LINE_TRACKING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint8_t x1_black;
  uint8_t x2_black;
  uint8_t x3_black;
  uint8_t x4_black;
  /* GPIO snapshots carry their acquisition boundary. Synthetic readings must
     zero-initialize these fields and are observed at compute time. */
  uint32_t sampled_ms;
  uint8_t sampled_time_valid;
} LineTrackingReading;

typedef enum
{
  LINE_ACTION_STOP = 0,
  LINE_ACTION_FORWARD,
  LINE_ACTION_LEFT_ADJUST,
  LINE_ACTION_RIGHT_ADJUST,
  LINE_ACTION_LEFT_SHARP,
  LINE_ACTION_RIGHT_SHARP,
  LINE_ACTION_CROSSING,
  LINE_ACTION_SEARCH_LEFT,
  LINE_ACTION_SEARCH_RIGHT
} LineTrackingAction;

typedef struct
{
  int32_t left_cps;
  int32_t right_cps;
  LineTrackingAction action;
  /* valid=0 while recovery directly owns DriveBase speed/brake commands. */
  uint8_t valid;
} LineTrackingCommand;

void line_tracking_init(void);
void line_tracking_reset(void);
/* Apply a freshly computed line command after the owner's forward speed cap.
   Rebind bounded turn assistance to the final targets. valid=0 keeps recovery
   ownership. Nonzero commands respect drive faults, braking and position
   ownership; a zero forward cap remains an explicit stop request. */
void line_tracking_apply_command(const LineTrackingCommand *command, int16_t forward_limit_pwm);
/* enable=1：尚未见过黑线时允许无黑线直行。窄中线短缺口先低速跨越，再丢线才静音搜索。
   仅外侧识黑时内侧停、外侧低速前进；相邻双探头正向差速；横线多点优先低速穿越。
   同一最外侧单独持续识黑 120 ms 后以两侧反向强修正；其他原始状态立即解除。
   短缺口确认后丢线使用两侧等大反向目标搜索，保持静音。
   三路相邻识黑只更新方向提示，不立即原地转向；双外侧/非相邻组合不产生新提示。
   近期侧向提示可跨越短多黑区域保留至原采样后 400 ms；中心/反侧证据可使其失效。
   窄中间线重复确认后直接滚动接线。STOP/reset 取消；驱动观察策略见 DriveBase。 */
void line_tracking_set_no_line_forward(uint8_t enable);
/* enable=1: use filtered PD differential steering as the line-position outer
   loop. Wheel-speed feedback remains in DriveBase. */
void line_tracking_set_smooth_mode(uint8_t enable);
/* 100 keeps the normal KEY2 middle steering gain; 200 doubles KEY1's middle
   steering before PWM saturation. Explicit outer/adjacent CPS are not boosted. */
void line_tracking_set_turn_gain_percent(uint16_t percent);
/* Pass snapshots to compute in acquisition order. ISR history newer than a
   snapshot stays queued for the next snapshot instead of being overwritten. */
LineTrackingReading line_tracking_read(void);
/* Direction evidence is independent of immediate corner permission.
   Adjacent triples X2+X1+X3 / X1+X3+X4 report -1/+1 but still drive straight
   through the crossing guard. Both outers and nonadjacent pairs report 0. */
int8_t line_tracking_direction_evidence(const LineTrackingReading *reading);
LineTrackingAction line_tracking_compute(const LineTrackingReading *reading,
                                         int16_t base_speed,
                                         LineTrackingCommand *command);

#ifdef __cplusplus
}
#endif

#endif /* __LINE_TRACKING_H */
