#ifndef LINE_TURN_PULSE_H
#define LINE_TURN_PULSE_H
#include <stdint.h>
#define LINE_TURN_PULSE_DRIVE_MS 24U
typedef struct {
  uint32_t started_ms;
  int32_t start_count[4];
  int8_t direction[4];
  uint8_t active, reversing, ended_mask;
} LineTurnPulseState;
/* 24ms drive maximum per 40ms cycle, ratio/travel cutoffs, shared coast tail. */
void LineTurnPulse_Update(LineTurnPulseState *state, uint32_t now,
                          const int32_t target[4], const int32_t count[4],
                          uint8_t degraded_mask, int16_t output[4]);
#endif
