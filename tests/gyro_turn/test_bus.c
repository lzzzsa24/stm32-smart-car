#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "mpu6050_yaw.h"
TestDwt test_dwt;
TestDebug test_debug;
uint32_t SystemCoreClock = 72000000U;
static uint8_t replies[1024], wave[1200], sda_level = 1, stretch;
static unsigned reply_count, reply_index, wave_count;
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *gpio)
{
  if (port == GPIOB) assert(gpio->Pin == (GPIO_PIN_10 | GPIO_PIN_11) && gpio->Mode == GPIO_MODE_OUTPUT_OD);
  else assert(port == GPIOE && gpio->Pin == GPIO_PIN_0);
}
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
  test_dwt.CYCCNT += 72;
  if (port == GPIOE) { assert(pin == GPIO_PIN_0 && state == GPIO_PIN_RESET); return; }
  assert(port == GPIOB && !(pin & ~(GPIO_PIN_10 | GPIO_PIN_11)));
  if (pin & GPIO_PIN_11) sda_level = (uint8_t)state;
  if ((pin & GPIO_PIN_10) && state == GPIO_PIN_SET)
  { assert(wave_count < sizeof(wave)); wave[wave_count++] = sda_level; }
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
  test_dwt.CYCCNT += 72;
  assert(port == GPIOB);
  if (pin == GPIO_PIN_10) return stretch ? GPIO_PIN_RESET : GPIO_PIN_SET;
  assert(pin == GPIO_PIN_11 && reply_index < reply_count);
  return replies[reply_index++] ? GPIO_PIN_SET : GPIO_PIN_RESET;
}
static void reset(void)
{ reply_index = reply_count = wave_count = 0; stretch = 0; }
static void reply(uint8_t value) { replies[reply_count++] = value; }
static void byte_reply(uint8_t value)
{ unsigned i; for (i = 0; i < 8; ++i) reply((uint8_t)((value >> (7 - i)) & 1)); }
static uint8_t sent(unsigned at)
{ unsigned i; uint8_t v = 0; for (i = 0; i < 8; ++i) v = (uint8_t)(v * 2 + wave[at + i]); return v; }
int main(void)
{
  uint8_t bytes[96];
  uint32_t start;
  unsigned i;
  test_dwt.CYCCNT = 123456;
  reply(1); assert(MpuBus_Init()); assert(test_dwt.CYCCNT >= 123456);
  reset(); reply(1); reply(0); reply(0); reply(0);
  assert(MpuBus_Write(0x19, 9));
  assert(reply_index == reply_count && wave_count == 29);
  assert(sent(1) == 0xd0 && sent(10) == 0x19 && sent(19) == 9);
  reset(); reply(1); reply(0); reply(0); reply(1); reply(0); byte_reply(0xab); byte_reply(0xcd);
  assert(MpuBus_Read(0x72, bytes, 2)); assert(bytes[0] == 0xab && bytes[1] == 0xcd);
  assert(sent(1) == 0xd0 && sent(10) == 0x72 && sent(20) == 0xd1);
  assert(wave[37] == 0 && wave[46] == 1); /* ACK first byte, NACK last */
  reset(); reply(1); reply(1); assert(!MpuBus_Write(0x19, 9)); /* NACK */
  reset(); reply(0); assert(!MpuBus_Write(0x19, 9)); /* stuck SDA */
  reset(); stretch = 1; start = test_dwt.CYCCNT;
  assert(!MpuBus_Write(0x19, 9));
  assert(test_dwt.CYCCNT - start >= 12000U * 72U);
  assert(test_dwt.CYCCNT - start < 12100U * 72U);
  reset(); test_dwt.CYCCNT = UINT32_MAX - 200U;
  reply(1); reply(0); reply(0); reply(1); reply(0);
  for (i = 0; i < sizeof(bytes); ++i) byte_reply((uint8_t)i);
  assert(MpuBus_Read(0x74, bytes, sizeof(bytes)));
  for (i = 0; i < sizeof(bytes); ++i) assert(bytes[i] == i);
  puts("PASS: bus pin scope, address/register bits, repeated START, ACK/NACK, 96-byte FIFO, bounded stuck bus and DWT wrap");
  return 0;
}
