#include "main.h"
#include "line_sensor_sample.h"

/* Override the HAL weak extension point, preserving its exact time update.
   The existing SysTick_Handler calls this at the default 1-ms HAL frequency.
   No shared interrupt, encoder, GPIO or main-loop source needs modification. */
void HAL_IncTick(void)
{
  uwTick += (uint32_t)uwTickFreq;
  LineSensorSample_Tick(uwTick);
}
