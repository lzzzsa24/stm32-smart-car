#include "dfplayer_mini.h"

#include "dfplayer_protocol.h"
#include "main.h"

#define DFPLAYER_UART_BAUD                9600UL
#define DFPLAYER_COMMAND_NEXT               0x01U
#define DFPLAYER_COMMAND_PREVIOUS           0x02U
#define DFPLAYER_COMMAND_PLAY_PHYSICAL      0x03U
#define DFPLAYER_COMMAND_SET_VOLUME         0x06U
#define DFPLAYER_COMMAND_START              0x0DU
#define DFPLAYER_COMMAND_PAUSE              0x0EU
#define DFPLAYER_COMMAND_PLAY_MP3_FOLDER    0x12U
#define DFPLAYER_COMMAND_STOP               0x16U
#define DFPLAYER_COMMAND_LOOP_CURRENT       0x19U
#define DFPLAYER_COMMAND_TRACK_FINISHED_USB 0x3CU
#define DFPLAYER_COMMAND_TRACK_FINISHED_SD  0x3DU
#define DFPLAYER_COMMAND_QUERY_SD_TRACK     0x4CU

#define DFPLAYER_TRANSPORT_QUEUE_CAPACITY      8U
#define DFPLAYER_TRACK_QUERY_DELAY_MS         240U

typedef struct
{
  uint8_t command;
  uint16_t parameter;
} DfPlayerQueuedCommand;

static volatile uint8_t tx_packet[DFPLAYER_PROTOCOL_PACKET_SIZE];
static volatile uint8_t tx_index;
static volatile uint8_t tx_active;
static volatile uint8_t tx_command;

static volatile uint8_t rx_packet[DFPLAYER_PROTOCOL_PACKET_SIZE];
static volatile uint8_t rx_index;
static volatile uint16_t rx_reported_track;
static volatile uint8_t rx_track_ready;

static DfPlayerQueuedCommand transport_queue[DFPLAYER_TRANSPORT_QUEUE_CAPACITY];
static uint8_t transport_head;
static uint8_t transport_tail;
static uint8_t transport_count;

static uint8_t player_ready;
static uint8_t volume_pending;
static uint8_t requested_volume;
static uint8_t loop_pending;
static uint8_t loop_current_enabled;
static uint8_t stop_pending;
static uint8_t track_query_pending;
static uint16_t current_track;
static uint8_t track_changed;
static DfPlayerMiniPlaybackState playback_state;
static uint32_t ready_at_ms;
static uint32_t next_command_at_ms;
static uint32_t track_query_not_before_ms;

static uint8_t tick_reached(uint32_t now_ms, uint32_t deadline_ms)
{
  return (int32_t)(now_ms - deadline_ms) >= 0 ? 1U : 0U;
}

static uint8_t queue_push(uint8_t command, uint16_t parameter)
{
  if (transport_count >= DFPLAYER_TRANSPORT_QUEUE_CAPACITY)
  {
    return 0U;
  }

  transport_queue[transport_tail].command = command;
  transport_queue[transport_tail].parameter = parameter;
  transport_tail = (uint8_t)((transport_tail + 1U) %
                             DFPLAYER_TRANSPORT_QUEUE_CAPACITY);
  ++transport_count;
  return 1U;
}

static uint8_t queue_pop(DfPlayerQueuedCommand *queued)
{
  if (queued == 0 || transport_count == 0U)
  {
    return 0U;
  }

  *queued = transport_queue[transport_head];
  transport_head = (uint8_t)((transport_head + 1U) %
                             DFPLAYER_TRANSPORT_QUEUE_CAPACITY);
  --transport_count;
  return 1U;
}

static void queue_clear(void)
{
  transport_head = 0U;
  transport_tail = 0U;
  transport_count = 0U;
}

static void mark_track_changed(uint16_t track_number)
{
  if (track_number == 0U)
  {
    return;
  }
  current_track = track_number;
  track_changed = 1U;
}

static void request_track_query(void)
{
  track_query_pending = 1U;
  track_query_not_before_ms = HAL_GetTick() + DFPLAYER_TRACK_QUERY_DELAY_MS;
}

static uint8_t command_controls_playback(uint8_t command)
{
  return (command == DFPLAYER_COMMAND_NEXT ||
          command == DFPLAYER_COMMAND_PREVIOUS ||
          command == DFPLAYER_COMMAND_PLAY_PHYSICAL ||
          command == DFPLAYER_COMMAND_START ||
          command == DFPLAYER_COMMAND_PAUSE ||
          command == DFPLAYER_COMMAND_PLAY_MP3_FOLDER ||
          command == DFPLAYER_COMMAND_LOOP_CURRENT) ? 1U : 0U;
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

static uint8_t received_packet_valid(const uint8_t *packet)
{
  uint16_t checksum;
  uint16_t expected;
  uint8_t index;

  if (packet == 0 || packet[0] != 0x7EU || packet[1] != 0xFFU ||
      packet[2] != 0x06U || packet[9] != 0xEFU)
  {
    return 0U;
  }

  checksum = 0U;
  for (index = 1U; index <= 6U; ++index)
  {
    checksum = (uint16_t)(checksum + packet[index]);
  }
  checksum = (uint16_t)(0U - checksum);
  expected = (uint16_t)(((uint16_t)packet[7] << 8U) | packet[8]);
  return checksum == expected ? 1U : 0U;
}

static void receive_byte(uint8_t value)
{
  uint8_t packet[DFPLAYER_PROTOCOL_PACKET_SIZE];
  uint8_t command;
  uint16_t parameter;
  uint8_t index;

  if (rx_index == 0U && value != 0x7EU)
  {
    return;
  }

  rx_packet[rx_index++] = value;
  if (rx_index < DFPLAYER_PROTOCOL_PACKET_SIZE)
  {
    return;
  }

  for (index = 0U; index < DFPLAYER_PROTOCOL_PACKET_SIZE; ++index)
  {
    packet[index] = rx_packet[index];
  }
  rx_index = 0U;
  if (received_packet_valid(packet) == 0U)
  {
    return;
  }

  command = packet[3];
  parameter = (uint16_t)(((uint16_t)packet[5] << 8U) | packet[6]);
  if ((command == DFPLAYER_COMMAND_QUERY_SD_TRACK ||
       command == DFPLAYER_COMMAND_TRACK_FINISHED_SD ||
       command == DFPLAYER_COMMAND_TRACK_FINISHED_USB) &&
      parameter != 0U)
  {
    rx_reported_track = parameter;
    __DMB();
    rx_track_ready = 1U;
  }
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
  UART4->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE |
               USART_CR1_UE;

  tx_index = 0U;
  tx_active = 0U;
  tx_command = 0U;
  rx_index = 0U;
  rx_reported_track = 0U;
  rx_track_ready = 0U;
  queue_clear();
  player_ready = 0U;
  requested_volume = DFPLAYER_MINI_DEFAULT_VOLUME;
  volume_pending = 1U;
  loop_current_enabled = DFPLAYER_MINI_DEFAULT_LOOP_CURRENT != 0U ? 1U : 0U;
  loop_pending = 1U;
  stop_pending = 0U;
  track_query_pending = 0U;
  current_track = 1U;
  track_changed = 0U;
  playback_state = DFPLAYER_MINI_STOPPED;
  ready_at_ms = HAL_GetTick() + DFPLAYER_MINI_BOOT_DELAY_MS;
  next_command_at_ms = ready_at_ms;
  track_query_not_before_ms = ready_at_ms;

  HAL_NVIC_ClearPendingIRQ(UART4_IRQn);
  HAL_NVIC_SetPriority(UART4_IRQn, 3U, 1U);
  HAL_NVIC_EnableIRQ(UART4_IRQn);
}

void DfPlayerMini_Task(uint32_t now_ms)
{
  uint8_t reported_ready;
  uint16_t reported_track;
  DfPlayerQueuedCommand queued;

  HAL_NVIC_DisableIRQ(UART4_IRQn);
  reported_ready = rx_track_ready;
  reported_track = rx_reported_track;
  rx_track_ready = 0U;
  HAL_NVIC_EnableIRQ(UART4_IRQn);
  if (reported_ready != 0U && reported_track != 0U &&
      reported_track != current_track)
  {
    mark_track_changed(reported_track);
  }

  if (player_ready == 0U)
  {
    if (tick_reached(now_ms, ready_at_ms) == 0U)
    {
      return;
    }
    player_ready = 1U;
  }

  if (tx_active != 0U || tick_reached(now_ms, next_command_at_ms) == 0U)
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
  else if (queue_pop(&queued) != 0U)
  {
    start_command(queued.command, queued.parameter, now_ms);
  }
  else if (loop_pending != 0U)
  {
    loop_pending = 0U;
    start_command(DFPLAYER_COMMAND_LOOP_CURRENT,
                  loop_current_enabled != 0U ? 0U : 1U,
                  now_ms);
  }
  else if (track_query_pending != 0U &&
           tick_reached(now_ms, track_query_not_before_ms) != 0U)
  {
    track_query_pending = 0U;
    start_command(DFPLAYER_COMMAND_QUERY_SD_TRACK, 0U, now_ms);
  }
}

uint8_t DfPlayerMini_PlayMp3Track(uint16_t track_number)
{
  if (track_number == 0U ||
      queue_push(DFPLAYER_COMMAND_PLAY_MP3_FOLDER, track_number) == 0U)
  {
    return 0U;
  }

  mark_track_changed(track_number);
  playback_state = DFPLAYER_MINI_PLAYING;
  loop_pending = 1U;
  stop_pending = 0U;
  request_track_query();
  return 1U;
}

uint8_t DfPlayerMini_SetResumeTrack(uint16_t track_number)
{
  if (track_number == 0U || playback_state != DFPLAYER_MINI_STOPPED ||
      transport_count != 0U)
  {
    return 0U;
  }
  current_track = track_number;
  return 1U;
}

uint8_t DfPlayerMini_TogglePlayPause(void)
{
  uint8_t command;
  uint16_t parameter = 0U;

  if (playback_state == DFPLAYER_MINI_PLAYING)
  {
    command = DFPLAYER_COMMAND_PAUSE;
  }
  else if (playback_state == DFPLAYER_MINI_PAUSED)
  {
    command = DFPLAYER_COMMAND_START;
  }
  else
  {
    command = DFPLAYER_COMMAND_PLAY_PHYSICAL;
    parameter = current_track;
  }

  if (queue_push(command, parameter) == 0U)
  {
    return 0U;
  }

  if (playback_state == DFPLAYER_MINI_PLAYING)
  {
    playback_state = DFPLAYER_MINI_PAUSED;
  }
  else
  {
    playback_state = DFPLAYER_MINI_PLAYING;
  }
  if (command == DFPLAYER_COMMAND_PLAY_PHYSICAL)
  {
    loop_pending = 1U;
    request_track_query();
  }
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
  uint16_t guessed_track;

  if (queue_push(DFPLAYER_COMMAND_NEXT, 0U) == 0U)
  {
    return 0U;
  }
  guessed_track = current_track == 0xFFFFU ? 1U :
                  (uint16_t)(current_track + 1U);
  mark_track_changed(guessed_track);
  playback_state = DFPLAYER_MINI_PLAYING;
  loop_pending = 1U;
  stop_pending = 0U;
  request_track_query();
  return 1U;
}

uint8_t DfPlayerMini_Previous(void)
{
  uint16_t guessed_track;

  if (queue_push(DFPLAYER_COMMAND_PREVIOUS, 0U) == 0U)
  {
    return 0U;
  }
  guessed_track = current_track > 1U ? (uint16_t)(current_track - 1U) : 1U;
  mark_track_changed(guessed_track);
  playback_state = DFPLAYER_MINI_PLAYING;
  loop_pending = 1U;
  stop_pending = 0U;
  request_track_query();
  return 1U;
}

DfPlayerMiniPlaybackState DfPlayerMini_GetPlaybackState(void)
{
  return playback_state;
}

uint16_t DfPlayerMini_GetCurrentTrack(void)
{
  return current_track;
}

uint8_t DfPlayerMini_TakeTrackChanged(uint16_t *track_number)
{
  if (track_number == 0 || track_changed == 0U)
  {
    return 0U;
  }
  *track_number = current_track;
  track_changed = 0U;
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
      (tx_active != 0U && command_controls_playback(tx_command) != 0U) ?
      1U : 0U;

  if (playback_state != DFPLAYER_MINI_STOPPED || transport_count != 0U ||
      playback_command_is_transmitting != 0U)
  {
    queue_clear();
    loop_pending = 0U;
    track_query_pending = 0U;
    playback_state = DFPLAYER_MINI_STOPPED;
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
    uint8_t received = (uint8_t)UART4->DR;
    if ((status & (USART_SR_ORE | USART_SR_NE | USART_SR_FE |
                   USART_SR_PE)) == 0U)
    {
      receive_byte(received);
    }
    else
    {
      rx_index = 0U;
    }
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
