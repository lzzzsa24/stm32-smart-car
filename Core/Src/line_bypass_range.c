#include "line_bypass_range.h"
#include "ultrasonic.h"
#include "main.h"

#define RANGE_INTERVAL_MS 60U
#define RANGE_FRESH_MS 250U
static uint8_t was_forward, request_valid, obstacle;
static uint32_t trigger_ms, sample_ms;

void LineBypassRange_Reset(void)
{
  was_forward = request_valid = obstacle = 0U;
  trigger_ms = HAL_GetTick() - RANGE_INTERVAL_MS;
  sample_ms = HAL_GetTick();
}

uint8_t LineBypassRange_Task(uint8_t forward_active, uint16_t obstacle_cm)
{
  uint16_t cm = 0U;
  uint8_t result;
  uint32_t now = HAL_GetTick();
  if (!forward_active || !was_forward)
  {
    request_valid = obstacle = 0U;
    trigger_ms = now - RANGE_INTERVAL_MS;
  }
  was_forward = forward_active != 0U;
  Ultrasonic_Task();
  result = Ultrasonic_GetResult(&cm);
  if (result != ULTRASONIC_RESULT_NONE)
  {
    if (forward_active && request_valid && result == ULTRASONIC_RESULT_OK &&
        now - trigger_ms <= RANGE_FRESH_MS)
    { obstacle = cm <= obstacle_cm; sample_ms = now; }
    request_valid = 0U;
  }
  if (now - sample_ms > RANGE_FRESH_MS) obstacle = 0U;
  if (forward_active && !Ultrasonic_IsBusy() &&
      now - trigger_ms >= RANGE_INTERVAL_MS && Ultrasonic_Start())
  { trigger_ms = now; request_valid = 1U; }
  return obstacle;
}
