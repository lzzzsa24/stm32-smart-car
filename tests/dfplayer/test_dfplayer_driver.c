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

static void inject_feedback(uint8_t command, uint16_t parameter)
{
  uint8_t packet[DFPLAYER_PROTOCOL_PACKET_SIZE];
  uint8_t index;

  DfPlayerProtocol_Build(command, parameter, packet);
  for (index = 0U; index < DFPLAYER_PROTOCOL_PACKET_SIZE; ++index)
  {
    UART4->DR = packet[index];
    UART4->SR = USART_SR_RXNE;
    DfPlayerMini_UART4_IRQHandler();
  }
  UART4->SR = 0U;
}

int main(void)
{
  uint32_t now = 0U;

  memset(&fake_uart4, 0, sizeof(fake_uart4));
  DfPlayerMini_Init();
  assert(DfPlayerMini_GetVolume() == 20U);
  assert((UART4->CR1 & USART_CR1_RXNEIE) != 0U);
  assert(DfPlayerMini_SetResumeTrack(7U) != 0U);
  assert(DfPlayerMini_GetCurrentTrack() == 7U);
  assert(DfPlayerMini_GetPlaybackState() == DFPLAYER_MINI_STOPPED);
  DfPlayerMini_Task(2999U);
  assert((UART4->CR1 & USART_CR1_TXEIE) == 0U);

  run_command_at(3000U, 0x06U, 20U);
  run_command_at(3080U, 0x19U, 0U);
  now = 3080U;

  assert(DfPlayerMini_AdjustVolume(2) != 0U);
  assert(DfPlayerMini_GetVolume() == 22U);
  now += 80U;
  run_command_at(now, 0x06U, 22U);

  assert(DfPlayerMini_AdjustVolume(-127) != 0U);
  assert(DfPlayerMini_GetVolume() == 0U);
  now += 80U;
  run_command_at(now, 0x06U, 0U);

  assert(DfPlayerMini_AdjustVolume(127) != 0U);
  assert(DfPlayerMini_GetVolume() == 30U);
  now += 80U;
  run_command_at(now, 0x06U, 30U);

  assert(DfPlayerMini_TogglePlayPause() != 0U);
  assert(DfPlayerMini_GetPlaybackState() == DFPLAYER_MINI_PLAYING);
  now += 80U;
  run_command_at(now, 0x03U, 7U);
  now += 80U;
  run_command_at(now, 0x19U, 0U);
  now += 80U;
  run_command_at(now, 0x4CU, 0U);
  inject_feedback(0x4CU, 7U);
  DfPlayerMini_Task(now + 1U);

  assert(DfPlayerMini_TogglePlayPause() != 0U);
  assert(DfPlayerMini_GetPlaybackState() == DFPLAYER_MINI_PAUSED);
  now += 80U;
  run_command_at(now, 0x0EU, 0U);
  assert(DfPlayerMini_TogglePlayPause() != 0U);
  assert(DfPlayerMini_GetPlaybackState() == DFPLAYER_MINI_PLAYING);
  now += 80U;
  run_command_at(now, 0x0DU, 0U);

  assert(DfPlayerMini_Next() != 0U);
  {
    uint16_t changed_track = 0U;
    assert(DfPlayerMini_TakeTrackChanged(&changed_track) != 0U);
    assert(changed_track == 8U);
  }
  now += 80U;
  run_command_at(now, 0x01U, 0U);
  now += 80U;
  run_command_at(now, 0x19U, 0U);
  now += 80U;
  run_command_at(now, 0x4CU, 0U);
  inject_feedback(0x4CU, 9U);
  DfPlayerMini_Task(now + 1U);
  {
    uint16_t changed_track = 0U;
    assert(DfPlayerMini_TakeTrackChanged(&changed_track) != 0U);
    assert(changed_track == 9U);
  }

  assert(DfPlayerMini_Previous() != 0U);
  {
    uint16_t changed_track = 0U;
    assert(DfPlayerMini_TakeTrackChanged(&changed_track) != 0U);
    assert(changed_track == 8U);
  }
  now += 80U;
  run_command_at(now, 0x02U, 0U);
  now += 80U;
  run_command_at(now, 0x19U, 0U);

  DfPlayerMini_Stop();
  assert(DfPlayerMini_GetPlaybackState() == DFPLAYER_MINI_STOPPED);
  now += 80U;
  run_command_at(now, 0x16U, 0U);

  puts("dfplayer queue, play/pause, generic next/previous, feedback, loop and stop passed");
  return 0;
}
