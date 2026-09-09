#include "sign_observation.h"
#include <stddef.h>

static uint32_t sequence, pause_ms;
static uint8_t sequence_valid, pause_allowed, pause_active, pause_started;

void SignObservation_Reset(void)
{
  sequence = pause_ms = 0U;
  sequence_valid = pause_allowed = pause_active = pause_started = 0U;
}
void SignObservation_AllowPause(uint8_t allowed) { pause_allowed = allowed; }
uint8_t SignObservation_Paused(uint32_t now)
{
  if (pause_active && now - pause_ms >= 2000U) pause_active = 0U;
  return pause_active;
}
void SignObservation_ObserveDetection(const VisionDetection *frame, uint32_t now)
{
  if (frame == NULL || (sequence_valid && frame->sequence == sequence)) return;
  sequence = frame->sequence;
  sequence_valid = 1U;
  if (now - frame->received_ms > 350U || frame->class_id < 0 || frame->class_id > 1 ||
      frame->score < 25U || frame->score > 100U || frame->center_x >= 320U || frame->center_y >= 240U)
    return;
  if (pause_allowed && (!pause_started || now - pause_ms >= 2500U))
  {
    pause_started = pause_active = 1U;
    pause_ms = now; /* fixed 2 seconds, never renewed by subsequent frames */
  }
}
