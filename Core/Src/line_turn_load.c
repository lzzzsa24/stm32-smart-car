#include "line_turn_load.h"

#define LOAD_CONFIRM_MS       40U
#define LOAD_MAX_EXTRA_PWM   600L
#define LOAD_RISE_PWM_PER_MS   5L
#define LOAD_MIN_CPS        1412L
#define LOAD_MAX_CPS        9000L

static int16_t update_load(LineTurnLoadState *s, uint8_t enabled,
                           int32_t target, int32_t measured, uint32_t elapsed,
                           uint32_t confirm_ms, int32_t maximum, int32_t rise)
{
  int32_t magnitude, desired, next;
  int64_t actual;
  if (!s) return 0;
  /* Reject stale samples as well as reversal, overspeed and low-speed pulses.
     Clear immediately on recovery; do not keep a timed open-loop kick. */
  if (!enabled || elapsed == 0U || elapsed > 60U ||
      target < -LOAD_MAX_CPS || target > LOAD_MAX_CPS) goto clear;
  magnitude = target < 0 ? -target : target;
  actual = target < 0 ? -(int64_t)measured : (int64_t)measured;
  if (magnitude < LOAD_MIN_CPS || actual < 0 ||
      (int64_t)actual * 100 >= (int64_t)magnitude * 85) goto clear;

  if (s->slow_ms < confirm_ms) s->slow_ms += elapsed;
  if (s->slow_ms < confirm_ms) return 0;
  desired = (magnitude - (int32_t)actual) / 2;
  if (desired > maximum) desired = maximum;
  next = s->extra_pwm + (int32_t)elapsed * rise;
  s->extra_pwm = (int16_t)(next < desired ? next : desired);
  return s->extra_pwm;
clear:
  s->slow_ms = 0U;
  s->extra_pwm = 0;
  return 0;
}

int16_t LineTurnLoad_Update(LineTurnLoadState *s, uint8_t enabled,
                           int32_t target, int32_t measured, uint32_t elapsed)
{
  return update_load(s, enabled, target, measured, elapsed,
                     LOAD_CONFIRM_MS, LOAD_MAX_EXTRA_PWM, LOAD_RISE_PWM_PER_MS);
}

int16_t LineTurnLoad_UpdateFast(LineTurnLoadState *s, uint8_t enabled,
                               int32_t target, int32_t measured, uint32_t elapsed)
{
  /* Continuous PI plus prompt deficit-dependent assistance, with no off
     interval or timed kick after the encoder reports recovery. */
  return update_load(s, enabled, target, measured, elapsed, 20U, 900L, 20L);
}
