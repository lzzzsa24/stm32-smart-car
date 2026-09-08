#include "dfplayer_mini.h"

#include "dfplayer_protocol.h"
#include "main.h"

#define DFPLAYER_UART_BAUD              9600UL
#define DFPLAYER_COMMAND_NEXT             0x01U
#define DFPLAYER_COMMAND_SET_VOLUME       0x06U
#define DFPLAYER_COMMAND_PLAY_MP3_FOLDER  0x12U
#define DFPLAYER_COMMAND_STOP             0x16U
#define DFPLAYER_COMMAND_LOOP_CURRENT      0x19U

static volatile uint8_t tx_packet[DFPLAYER_PROTOCOL_PACKET_SIZE];
static volatile uint8_t tx_index;
static volatile uint8_t tx_active;
static volatile uint8_t tx_command;

static uint8_t player_ready;
static uint8_t volume_pending;
static uint8_t requested_volume;
static uint8_t play_pending;
static uint16_t requested_track;
static uint8_t next_pending_count;
static uint8_t loop_pending;
static uint8_t loop_current_enabled;
static uint8_t stop_pending;
static uint8_t playback_requested;
static uint32_t ready_at_ms;
static uint32_t next_command_at_ms;

static uint8_t tick_reached(uint32_t now_ms, uint32_t deadline_ms)
{
  return (int32_t)(now_ms - deadline_ms) >= 0 ? 1U : 0U;
}

static void start_command(uint8_t command,
                          uint16_t parameter,
                          uint32_t now_ms)
{
  uint8_t packet[DFPLAYER_PROTOCOL_PACKET_SIZE];
  uint8_t index;

  DfPlayerProtocol_Build(command, parameter, packet);
  for (index = 0U; index < DFPLAYER_PROTOCOL_PACKET_SIZE; ++index)
  {
    tx_packet[index] = packet[index];
  }

  tx_command = command;
  tx_index = 0U;
  tx_active = 1U;
  next_command_at_ms = now_ms + DFPLAYER_MINI_COMMAND_GAP_MS;
  __DMB();
  UART4->CR1 |= USART_CR1_TXEIE;
}

void DfPlayerMini_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  uint32_t pclk;

  HAL_NVIC_DisableIRQ(UART4_IRQn);
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_UART4_CLK_ENABLE();

  gpio.Pin = GPIO_PIN_10;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOC, &gpio);

  gpio.Pin = GPIO_PIN_11;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &gpio);

  pclk = HAL_RCC_GetPCLK1Freq();
  UART4->CR1 = 0U;
  UART4->CR2 = 0U;
  UART4->CR3 = 0U;
  UART4->BRR = (pclk + (DFPLAYER_UART_BAUD / 2UL)) /
               DFPLAYER_UART_BAUD;
  UART4->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

  tx_index = 0U;
  tx_active = 0U;
  tx_command = 0U;
  player_ready = 0U;
  requested_volume = DFPLAYER_MINI_DEFAULT_VOLUME;
  volume_pending = 1U;
  play_pending = 0U;
  requested_track = 0U;
  next_pending_count = 0U;
  loop_current_enabled = DFPLAYER_MINI_DEFAULT_LOOP_CURRENT != 0U ? 1U : 0U;
  loop_pending = 1U;
  stop_pending = 0U;
  playback_requested = 0U;
  ready_at_ms = HAL_GetTick() + DFPLAYER_MINI_BOOT_DELAY_MS;
  next_command_at_ms = ready_at_ms;

  HAL_NVIC_ClearPendingIRQ(UART4_IRQn);
  HAL_NVIC_SetPriority(UART4_IRQn, 3U, 1U);
  HAL_NVIC_EnableIRQ(UART4_IRQn);
}

void DfPlayerMini_Task(uint32_t now_ms)
{
  uint32_t status = UART4->SR;

  /* Feedback is optional for this application.  Drain unsolicited card/status
     bytes and clear receive errors so they cannot accumulate indefinitely. */
  if ((status & (USART_SR_RXNE | USART_SR_ORE | USART_SR_NE |
                 USART_SR_FE | USART_SR_PE)) != 0U)
  {
    (void)UART4->DR;
  }

  if (player_ready == 0U)
  {
    if (tick_reached(now_ms, ready_at_ms) == 0U)
    {
      return;
    }
    player_ready = 1U;
  }

  if (tx_active != 0U ||
      tick_reached(now_ms, next_command_at_ms) == 0U)
  {
    return;
  }

  if (stop_pending != 0U)
  {
    stop_pending = 0U;
    start_command(DFPLAYER_COMMAND_STOP, 0U, now_ms);
  }
  else if (volume_pending != 0U)
  {
    volume_pending = 0U;
    start_command(DFPLAYER_COMMAND_SET_VOLUME,
                  (uint16_t)requested_volume,
                  now_ms);
  }
  else if (play_pending != 0U)
  {
    uint16_t track = requested_track;
    play_pending = 0U;
    start_command(DFPLAYER_COMMAND_PLAY_MP3_FOLDER, track, now_ms);
  }
  else if (next_pending_count != 0U)
  {
    --next_pending_count;
    start_command(DFPLAYER_COMMAND_NEXT, 0U, now_ms);
  }
  else if (loop_pending != 0U)
  {
    loop_pending = 0U;
    start_command(DFPLAYER_COMMAND_LOOP_CURRENT,
                  loop_current_enabled != 0U ? 0U : 1U,
                  now_ms);
  }
}

uint8_t DfPlayerMini_PlayMp3Track(uint16_t track_number)
{
  if (track_number == 0U)
  {
    return 0U;
  }

  requested_track = track_number;
  play_pending = 1U;
  playback_requested = 1U;
  loop_pending = 1U;
  stop_pending = 0U;
  return 1U;
}

uint8_t DfPlayerMini_SetVolume(uint8_t volume)
{
  if (volume > 30U)
  {
    return 0U;
  }

  requested_volume = volume;
  volume_pending = 1U;
  return 1U;
}

uint8_t DfPlayerMini_AdjustVolume(int8_t delta)
{
  int16_t adjusted = (int16_t)requested_volume + (int16_t)delta;

  if (adjusted < 0)
  {
    adjusted = 0;
  }
  else if (adjusted > 30)
  {
    adjusted = 30;
  }

  return DfPlayerMini_SetVolume((uint8_t)adjusted);
}

uint8_t DfPlayerMini_GetVolume(void)
{
  return requested_volume;
}

uint8_t DfPlayerMini_Next(void)
{
  if (next_pending_count != 0xFFU)
  {
    ++next_pending_count;
  }
  playback_requested = 1U;
  loop_pending = 1U;
  stop_pending = 0U;
  return 1U;
}

uint8_t DfPlayerMini_SetLoopCurrent(uint8_t enabled)
{
  loop_current_enabled = enabled != 0U ? 1U : 0U;
  loop_pending = 1U;
  return 1U;
}

void DfPlayerMini_Stop(void)
{
  uint8_t playback_command_is_transmitting =
      (tx_active != 0U &&
       (tx_command == DFPLAYER_COMMAND_PLAY_MP3_FOLDER ||
        tx_command == DFPLAYER_COMMAND_NEXT ||
        tx_command == DFPLAYER_COMMAND_LOOP_CURRENT)) ? 1U : 0U;

  if (play_pending != 0U || next_pending_count != 0U ||
      playback_requested != 0U || playback_command_is_transmitting != 0U)
  {
    play_pending = 0U;
    next_pending_count = 0U;
    loop_pending = 0U;
    playback_requested = 0U;
    if (player_ready != 0U)
    {
      stop_pending = 1U;
    }
  }
}

uint8_t DfPlayerMini_IsReady(void)
{
  return player_ready;
}

void DfPlayerMini_UART4_IRQHandler(void)
{
  uint32_t status = UART4->SR;

  if ((status & (USART_SR_RXNE | USART_SR_ORE | USART_SR_NE |
                 USART_SR_FE | USART_SR_PE)) != 0U)
  {
    (void)UART4->DR;
  }

  if ((status & USART_SR_TXE) != 0U &&
      (UART4->CR1 & USART_CR1_TXEIE) != 0U)
  {
    if (tx_index < DFPLAYER_PROTOCOL_PACKET_SIZE)
    {
      UART4->DR = tx_packet[tx_index++];
    }
    else
    {
      UART4->CR1 &= ~USART_CR1_TXEIE;
      tx_active = 0U;
    }
  }
}
