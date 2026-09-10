#include "promoted_sign_observation.h"
#include <stddef.h>

static uint32_t sequence, pause_ms;
static uint8_t sequence_valid, pause_allowed, pause_active, pause_started;
static uint8_t seeking_line;

void Promoted_SignObservation_Reset(void)
{
  sequence = pause_ms = 0U;
  sequence_valid = pause_allowed = pause_active = pause_started = 0U;
  seeking_line=0U;
}
void Promoted_SignObservation_AllowPause(uint8_t allowed) { pause_allowed = allowed; }
uint8_t Promoted_SignObservation_Paused(uint32_t now)
{
  if (pause_active && now - pause_ms >= 2000U) pause_active = 0U;
  return pause_active;
}
uint8_t Promoted_SignObservation_HoldingRoute(uint32_t now)
{ return Promoted_SignObservation_Paused(now) || seeking_line; }
void Promoted_SignObservation_UpdateLine(uint8_t mask, uint32_t now)
{
  mask &= 15U;
  if (pause_active && mask==0U)
  {
    pause_active=0U;
    seeking_line=1U;
  }
  /* One middle contact is enough. The caller stops in this same cycle and
     starts a full observation; outer-only contact must keep searching. */
  if (seeking_line && (mask & 6U)!=0U)
  {
    seeking_line=0U;
    pause_active=1U;
    pause_ms=now;
  }
}
uint8_t Promoted_SignObservation_SeekingLine(void) { return seeking_line; }
void Promoted_SignObservation_ObserveDetection(const VisionDetection *frame, uint32_t now,
                                     uint8_t minimum_score)
{
  if (frame == NULL || (sequence_valid && frame->sequence == sequence)) return;
  sequence = frame->sequence;
  sequence_valid = 1U;
  if (now - frame->received_ms > 350U || frame->class_id < 0 || frame->class_id > 1 ||
      frame->score < minimum_score || frame->score > 100U || frame->center_x >= 320U || frame->center_y >= 240U)
    return;
  if (!seeking_line && pause_allowed && (!pause_started || now - pause_ms >= 2500U))
  {
    pause_started = pause_active = 1U;
    pause_ms = now; /* fixed 2 seconds, never renewed by subsequent frames */
  }
}
