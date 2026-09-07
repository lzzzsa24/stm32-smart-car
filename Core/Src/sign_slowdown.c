#include "sign_slowdown.h"
#include <stddef.h>

static uint32_t black_ms, vision_ms, sequence;
static uint8_t black_valid, vision_valid, sequence_valid;

void SignSlowdown_Reset(void)
{
  black_valid = vision_valid = sequence_valid = 0U;
  black_ms = vision_ms = sequence = 0U;
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
  /* A no-target heartbeat is not recognition. No route vote is required:
     a single positive-score object of any supported class slows the car. */
  if (frame->class_id < 0 || frame->class_id > 4 ||
      frame->score == 0U || frame->score > 100U ||
      frame->center_x >= 320U || frame->center_y >= 240U ||
      now - frame->received_ms > SIGN_SLOWDOWN_FRAME_MAX_AGE_MS) return;
  vision_ms = frame->received_ms;
  vision_valid = 1U;
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
