#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dfplayer_mini.h"
#include "dfplayer_protocol.h"
#include "main.h"

USART_TypeDef fake_uart4;
GPIO_TypeDef fake_gpioc;
uint32_t fake_hal_tick;

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config)
{
  (void)port;
  (void)config;
}

uint32_t HAL_RCC_GetPCLK1Freq(void)
{
  return 36000000UL;
}

uint32_t HAL_GetTick(void)
{
  return fake_hal_tick;
}

static void expect_transmitted(uint8_t command, uint16_t parameter)
{
  uint8_t actual[DFPLAYER_PROTOCOL_PACKET_SIZE];
  uint8_t expected[DFPLAYER_PROTOCOL_PACKET_SIZE];
  uint8_t index;

  DfPlayerProtocol_Build(command, parameter, expected);
  assert((UART4->CR1 & USART_CR1_TXEIE) != 0U);
  for (index = 0U; index < DFPLAYER_PROTOCOL_PACKET_SIZE; ++index)
  {
    UART4->SR = USART_SR_TXE;
    DfPlayerMini_UART4_IRQHandler();
    actual[index] = (uint8_t)UART4->DR;
  }
  UART4->SR = USART_SR_TXE;
  DfPlayerMini_UART4_IRQHandler();
  assert((UART4->CR1 & USART_CR1_TXEIE) == 0U);
  assert(memcmp(actual, expected, sizeof(actual)) == 0);
}

static void run_command_at(uint32_t now_ms,
                           uint8_t command,
                           uint16_t parameter)
{
  fake_hal_tick = now_ms;
  DfPlayerMini_Task(now_ms);
  expect_transmitted(command, parameter);
}

int main(void)
{
  uint32_t now = 0U;

  memset(&fake_uart4, 0, sizeof(fake_uart4));
  DfPlayerMini_Init();
  assert(DfPlayerMini_GetVolume() == 10U);
  DfPlayerMini_Task(2999U);
  assert((UART4->CR1 & USART_CR1_TXEIE) == 0U);

  run_command_at(3000U, 0x06U, 10U);
  run_command_at(3080U, 0x19U, 0U);
  now = 3080U;

  assert(DfPlayerMini_AdjustVolume(2) != 0U);
  assert(DfPlayerMini_GetVolume() == 12U);
  now += 80U;
  run_command_at(now, 0x06U, 12U);

  assert(DfPlayerMini_AdjustVolume(-127) != 0U);
  assert(DfPlayerMini_GetVolume() == 0U);
  now += 80U;
  run_command_at(now, 0x06U, 0U);

  assert(DfPlayerMini_AdjustVolume(127) != 0U);
  assert(DfPlayerMini_GetVolume() == 30U);
  now += 80U;
  run_command_at(now, 0x06U, 30U);

  assert(DfPlayerMini_PlayMp3Track(1U) != 0U);
  now += 80U;
  run_command_at(now, 0x12U, 1U);
  now += 80U;
  run_command_at(now, 0x19U, 0U);

  assert(DfPlayerMini_Next() != 0U);
  now += 80U;
  run_command_at(now, 0x01U, 0U);
  now += 80U;
  run_command_at(now, 0x19U, 0U);

  DfPlayerMini_Stop();
  now += 80U;
  run_command_at(now, 0x16U, 0U);

  puts("dfplayer nonblocking queue, volume bounds, next, loop and stop passed");
  return 0;
}
