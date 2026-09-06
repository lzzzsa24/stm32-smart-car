#include "line_sensor_sample.h"
#include "main.h"

static volatile LineSensorSample samples[LINE_SENSOR_QUEUE_SIZE];
static volatile uint16_t head, tail, count;
static volatile uint32_t overwritten;
static volatile uint8_t started;

void LineSensorSample_Reset(void)
{
  uint32_t irq = __get_PRIMASK();
  __disable_irq();
  head = tail = count = 0U;
  overwritten = 0U;
  __set_PRIMASK(irq);
}
void LineSensorSample_Start(void)
{
  LineSensorSample_Reset();
  started = 1U;
}
void LineSensorSample_Tick(uint32_t now)
{
  uint8_t mask;
  if (!started) return; /* HAL tick starts before sensor GPIO initialization. */
  mask = (uint8_t)((HAL_GPIO_ReadPin(TRACK_X1_GPIO_Port, TRACK_X1_Pin) == GPIO_PIN_RESET ? 1U : 0U) |
      (HAL_GPIO_ReadPin(TRACK_X2_GPIO_Port, TRACK_X2_Pin) == GPIO_PIN_RESET ? 2U : 0U) |
      (HAL_GPIO_ReadPin(TRACK_X3_GPIO_Port, TRACK_X3_Pin) == GPIO_PIN_RESET ? 4U : 0U) |
      (HAL_GPIO_ReadPin(TRACK_X4_GPIO_Port, TRACK_X4_Pin) == GPIO_PIN_RESET ? 8U : 0U));
  if (count == LINE_SENSOR_QUEUE_SIZE)
  {
    tail = (uint16_t)((tail + 1U) % LINE_SENSOR_QUEUE_SIZE);
    --count;
    ++overwritten;
  }
  samples[head].time_ms = now;
  samples[head].mask = mask;
  head = (uint16_t)((head + 1U) % LINE_SENSOR_QUEUE_SIZE);
  ++count;
}
static uint8_t pop_sample(LineSensorSample *sample, uint8_t bounded, uint32_t through_ms)
{
  uint32_t irq = __get_PRIMASK();
  uint8_t available;
  __disable_irq();
  available = count != 0U;
  if (available && bounded && (int32_t)(samples[tail].time_ms - through_ms) > 0)
    available = 0U;
  if (available)
  {
    sample->time_ms = samples[tail].time_ms;
    sample->mask = samples[tail].mask;
    tail = (uint16_t)((tail + 1U) % LINE_SENSOR_QUEUE_SIZE);
    --count;
  }
  __set_PRIMASK(irq);
  return available;
}
uint8_t LineSensorSample_Pop(LineSensorSample *sample)
{ return pop_sample(sample, 0U, 0U); }
uint8_t LineSensorSample_PopThrough(LineSensorSample *sample, uint32_t through_ms)
{ return pop_sample(sample, 1U, through_ms); }
uint32_t LineSensorSample_Overwritten(void) { return overwritten; }
