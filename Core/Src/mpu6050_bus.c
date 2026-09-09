#include "mpu6050_yaw.h"
#include "main.h"

/* Bounded software I2C on the dedicated I2C2 socket. Does not touch the
   OLED's I2C1, timers or interrupts. DWT is shared with IR, never reset here. */
#define SCL GPIO_PIN_10
#define SDA GPIO_PIN_11
static uint32_t cycles_us, transaction_start;
static uint8_t failed;
static void delay_half(void)
{
  uint32_t start = DWT->CYCCNT;
  while ((uint32_t)(DWT->CYCCNT - start) < cycles_us * 3U) { __NOP(); }
}
static uint8_t expired(void)
{
  if ((uint32_t)(DWT->CYCCNT - transaction_start) > cycles_us * 12000U) failed = 1;
  return failed;
}
static void sda(uint8_t high)
{ HAL_GPIO_WritePin(GPIOB, SDA, high ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static void scl_low(void) { HAL_GPIO_WritePin(GPIOB, SCL, GPIO_PIN_RESET); }
static uint8_t scl_high(void)
{
  HAL_GPIO_WritePin(GPIOB, SCL, GPIO_PIN_SET);
  while (HAL_GPIO_ReadPin(GPIOB, SCL) == GPIO_PIN_RESET)
    if (expired()) return 0;
  delay_half();
  return !expired();
}
static void stop(void)
{
  scl_low(); sda(0); delay_half();
  (void)scl_high(); sda(1); delay_half();
}
static uint8_t start(void)
{
  sda(1); delay_half();
  if (!scl_high() || HAL_GPIO_ReadPin(GPIOB, SDA) == GPIO_PIN_RESET) return 0;
  sda(0); delay_half(); scl_low();
  return 1;
}
static uint8_t write_byte(uint8_t value)
{
  unsigned i;
  uint8_t ack;
  for (i = 0; i < 8; ++i)
  {
    sda((value & 0x80U) != 0); value <<= 1; delay_half();
    if (!scl_high()) return 0;
    scl_low();
  }
  sda(1); delay_half();
  if (!scl_high()) return 0;
  ack = HAL_GPIO_ReadPin(GPIOB, SDA) == GPIO_PIN_RESET;
  scl_low();
  return ack;
}
static uint8_t read_byte(uint8_t *value, uint8_t ack)
{
  unsigned i;
  *value = 0; sda(1);
  for (i = 0; i < 8; ++i)
  {
    delay_half(); if (!scl_high()) return 0;
    *value = (uint8_t)((*value << 1) | (HAL_GPIO_ReadPin(GPIOB, SDA) == GPIO_PIN_SET));
    scl_low();
  }
  sda(!ack); delay_half();
  if (!scl_high()) return 0;
  scl_low(); sda(1);
  return 1;
}
static void begin_transaction(void)
{ transaction_start = DWT->CYCCNT; failed = 0; }
uint8_t MpuBus_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  unsigned pulse;
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  cycles_us = SystemCoreClock / 1000000U;
  if (!cycles_us) return 0;
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_0, GPIO_PIN_RESET);
  gpio.Pin = GPIO_PIN_0;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &gpio);
  HAL_GPIO_WritePin(GPIOB, SCL | SDA, GPIO_PIN_SET);
  gpio.Pin = SCL | SDA;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);
  begin_transaction();
  /* Recover a partial byte at startup/explicit stationary recalibration. */
  for (pulse = 0; pulse < 9; ++pulse)
  { scl_low(); delay_half(); if (!scl_high()) break; }
  stop();
  return !failed && HAL_GPIO_ReadPin(GPIOB, SDA) == GPIO_PIN_SET;
}
uint8_t MpuBus_Write(uint8_t reg, uint8_t value)
{
  uint8_t ok;
  begin_transaction();
  ok = start() && write_byte(0xd0U) && write_byte(reg) && write_byte(value);
  stop();
  return ok && !failed;
}
uint8_t MpuBus_Read(uint8_t reg, uint8_t *data, uint16_t length)
{
  uint16_t i;
  uint8_t ok;
  begin_transaction();
  ok = start() && write_byte(0xd0U) && write_byte(reg) && start() && write_byte(0xd1U);
  for (i = 0; ok && i < length; ++i)
    ok = read_byte(data + i, i + 1U < length);
  stop();
  return ok && !failed;
}
