#include "sign_slowdown.h"
#include <stddef.h>

static uint32_t black_ms, vision_ms, sequence;
static uint8_t black_valid, vision_valid, sequence_valid;
static uint8_t pause_allowed, pause_active, pause_started;
static uint32_t pause_ms;

void SignSlowdown_AllowPause(uint8_t allowed) { pause_allowed = allowed; }
uint8_t SignSlowdown_Paused(uint32_t now)
{
  if (pause_active && now - pause_ms >= 2000U) pause_active = 0U;
  return pause_active;
}

void SignSlowdown_Reset(void)
{
  black_valid = vision_valid = sequence_valid = 0U;
  black_ms = vision_ms = sequence = 0U;
  pause_allowed = pause_active = pause_started = 0U;
  pause_ms = 0U;
}

void SignSlowdown_ObserveBlack(uint32_t sampled_ms)
{
  black_ms = sampled_ms;
  black_valid = 1U;
}

void SignSlowdown_ObserveDetection(const VisionDetection *frame, uint32_t now)
{
  if (frame == NULL ||
      (sequence_valid && frame->sequence == sequence)) return;
  sequence = frame->sequence;
  sequence_valid = 1U;
  if (now - frame->received_ms > SIGN_SLOWDOWN_FRAME_MAX_AGE_MS) return;
  if (frame->class_id == -1) return;
  /* A no-target heartbeat is not recognition. No route vote is required:
     a single positive-score object of any supported class slows the car. */
  if (frame->class_id < 0 || frame->class_id > 4 ||
      frame->score == 0U || frame->score > 100U ||
      frame->center_x >= 320U || frame->center_y >= 240U ||
      now - frame->received_ms > SIGN_SLOWDOWN_FRAME_MAX_AGE_MS) return;
  vision_ms = frame->received_ms;
  vision_valid = 1U;
  if (pause_allowed && (!pause_started || now - pause_ms >= 2500U) &&
      frame->class_id <= 1 && frame->score >= 25U)
  {
    pause_started = pause_active = 1U;
    pause_ms = now; /* never refreshed by subsequent frames */
  }
}

uint8_t SignSlowdown_Reasons(uint32_t now)
{
  if (black_valid && now - black_ms >= SIGN_SLOWDOWN_HOLD_MS)
    black_valid = 0U;
  if (vision_valid && now - vision_ms >= SIGN_SLOWDOWN_HOLD_MS)
    vision_valid = 0U;
  return (uint8_t)((black_valid ? SIGN_SLOWDOWN_BLACK : 0U) |
                   (vision_valid ? SIGN_SLOWDOWN_VISION : 0U));
}

int32_t SignSlowdown_ForwardCps(int16_t request)
{
  /* Sign requests are relative steering weights, not motor startup PWM.
     The shared PWM conversion floors every nonzero value <=2200 equally. */
  if (request <= 0) return 0;
  if (request >= 2300) return SIGN_SLOWDOWN_LIMIT_CPS;
  return (int32_t)request * SIGN_SLOWDOWN_LIMIT_CPS / 2300;
}

int32_t SignSlowdown_TargetLimit(uint8_t reasons, int16_t left_pwm, int16_t right_pwm)
{
  if ((left_pwm < 0 && right_pwm > 0) || (left_pwm > 0 && right_pwm < 0))
    return 0L;
  if (reasons & SIGN_SLOWDOWN_VISION) return SIGN_SLOWDOWN_VISION_LIMIT_CPS;
  if (reasons & SIGN_SLOWDOWN_BLACK) return SIGN_SLOWDOWN_BLACK_LIMIT_CPS;
  return SIGN_SLOWDOWN_LIMIT_CPS;
}
