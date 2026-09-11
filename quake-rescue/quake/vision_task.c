/*
 * K210 视觉任务帧接收（新增代码）
 *
 * 硬件沿用实验七：USART2 = PD5(TX)/PD6(RX)，3.3V TTL 与 K210 交叉连接并共地。
 * 本模块只用中断把字节收进环形缓冲，再在主循环里非阻塞解析，
 * 因此即使 K210 没接、乱码或半截数据也不会卡死寻线/避障。
 */

#include "vision_task.h"

#include <string.h>

#include "main.h"

#define VISION_TASK_BAUD             115200U
#define VISION_FRAME_BUFFER_SIZE       32U
#define VISION_RX_RING_SIZE           256U
#define VISION_MAX_BYTES_PER_POLL      64U
#define VISION_UART_ERROR_MASK (USART_SR_ORE | USART_SR_NE | \
                                USART_SR_FE | USART_SR_PE)

static volatile uint8_t rx_ring[VISION_RX_RING_SIZE];
static volatile uint8_t rx_head;
static volatile uint8_t rx_tail;
static volatile uint8_t rx_reset_pending;

static char frame_buffer[VISION_FRAME_BUFFER_SIZE];
static uint8_t frame_length;
static uint8_t collecting;

static QuakeVisionFrame latest_frame;
static uint8_t pending;
static uint32_t frame_sequence;
static uint32_t bad_frames;

/* ---------- 整数解析：带符号、范围校验、分隔符校验 ---------- */
static uint8_t parse_integer(const char **cursor, char delimiter,
                             int32_t minimum, int32_t maximum, int32_t *value)
{
  const char *text = *cursor;
  int32_t result = 0;
  int32_t sign = 1;
  uint8_t digits = 0U;

  if (*text == '-')
  {
    sign = -1;
    ++text;
  }
  while (*text >= '0' && *text <= '9')
  {
    if (result > 100000L)
    {
      return 0U;
    }
    result = result * 10L + (int32_t)(*text - '0');
    ++text;
    ++digits;
  }
  result *= sign;
  if (digits == 0U || result < minimum || result > maximum || *text != delimiter)
  {
    return 0U;
  }
  *cursor = delimiter != '\0' ? text + 1 : text;
  *value = result;
  return 1U;
}

/* ---------- 解析 "$T,mode,marker,cx,cy,area"（'#' 前） ---------- */
static uint8_t parse_t_frame(const char *frame, QuakeVisionFrame *output)
{
  const char *cursor = frame;
  int32_t mode, marker, cx, cy, area;

  if (cursor[0] != 'T' || cursor[1] != ',')
  {
    return 0U;
  }
  cursor += 2;

  if (!parse_integer(&cursor, ',', 0L, 3L, &mode) ||
      !parse_integer(&cursor, ',', -1L, 7L, &marker) ||
      !parse_integer(&cursor, ',', -1L, 319L, &cx) ||
      !parse_integer(&cursor, ',', -1L, 239L, &cy) ||
      !parse_integer(&cursor, '\0', -1L, 100000L, &area))
  {
    return 0U;
  }

  output->mode = (int8_t)mode;
  output->marker = (int8_t)marker;
  output->cx = (int16_t)cx;
  output->cy = (int16_t)cy;
  output->area = area;
  return 1U;
}

static void reset_frame_parser(void)
{
  frame_length = 0U;
  collecting = 0U;
}

static void consume_byte(uint8_t byte)
{
  QuakeVisionFrame frame;

  if (byte == '$')
  {
    frame_length = 0U;
    collecting = 1U;
    return;
  }
  if (collecting == 0U)
  {
    return;
  }
  if (byte == '#')
  {
    frame_buffer[frame_length] = '\0';
    collecting = 0U;
    if (parse_t_frame(frame_buffer, &frame) != 0U)
    {
      ++frame_sequence;
      frame.received_ms = HAL_GetTick();
      frame.sequence = frame_sequence;
      latest_frame = frame;
      pending = 1U;
    }
    else
    {
      ++bad_frames;
    }
    frame_length = 0U;
    return;
  }
  /* 换行/不可打印字符/溢出：丢弃本帧，下一帧仍可恢复。 */
  if (byte == '\r' || byte == '\n' || byte < 0x20U || byte > 0x7EU ||
      frame_length >= VISION_FRAME_BUFFER_SIZE - 1U)
  {
    ++bad_frames;
    frame_length = 0U;
    collecting = 0U;
    return;
  }
  frame_buffer[frame_length++] = (char)byte;
}

void vision_task_init(void)
{
  GPIO_InitTypeDef gpio = {0};
  uint32_t pclk1;
  uint32_t divider;

  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_USART2_CLK_ENABLE();
  __HAL_AFIO_REMAP_USART2_ENABLE();

  gpio.Pin = VISION_UART_TX_Pin;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(VISION_UART_TX_GPIO_Port, &gpio);

  gpio.Pin = VISION_UART_RX_Pin;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(VISION_UART_RX_GPIO_Port, &gpio);

  pclk1 = HAL_RCC_GetPCLK1Freq();
  divider = (pclk1 + (VISION_TASK_BAUD / 2U)) / VISION_TASK_BAUD;
  if (divider < 16U)
  {
    divider = 16U;
  }

  USART2->CR1 = 0U;
  USART2->CR2 = 0U;
  USART2->CR3 = 0U;
  USART2->BRR = divider;
  USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

  HAL_NVIC_SetPriority(USART2_IRQn, 3U, 0U);
  HAL_NVIC_EnableIRQ(USART2_IRQn);

  rx_head = 0U;
  rx_tail = 0U;
  rx_reset_pending = 0U;
  frame_length = 0U;
  collecting = 0U;
  pending = 0U;
  frame_sequence = 0U;
  bad_frames = 0U;
  memset(&latest_frame, 0, sizeof(latest_frame));
}

void vision_task_poll(void)
{
  uint32_t count = 0U;

  if (rx_reset_pending != 0U)
  {
    uint32_t cr1 = USART2->CR1;

    USART2->CR1 = cr1 & ~USART_CR1_RXNEIE;
    rx_tail = rx_head;
    rx_reset_pending = 0U;
    reset_frame_parser();
    USART2->CR1 = cr1;
  }

  while (rx_tail != rx_head && count < VISION_MAX_BYTES_PER_POLL)
  {
    uint8_t byte = rx_ring[rx_tail];

    rx_tail = (uint8_t)(rx_tail + 1U);
    consume_byte(byte);
    ++count;
  }
}

void vision_task_irq_handler(void)
{
  uint32_t status = USART2->SR;
  uint8_t byte;
  uint8_t next;

  if ((status & (USART_SR_RXNE | VISION_UART_ERROR_MASK)) == 0U)
  {
    return;
  }
  byte = (uint8_t)USART2->DR;
  if ((status & VISION_UART_ERROR_MASK) != 0U)
  {
    rx_reset_pending = 1U;
    return;
  }
  if ((status & USART_SR_RXNE) == 0U)
  {
    return;
  }
  next = (uint8_t)(rx_head + 1U);
  if (next == rx_tail)
  {
    rx_reset_pending = 1U;
    return;
  }
  rx_ring[rx_head] = byte;
  rx_head = next;
}

uint8_t vision_task_take_frame(QuakeVisionFrame *frame)
{
  vision_task_poll();
  if (pending == 0U || frame == 0)
  {
    return 0U;
  }
  *frame = latest_frame;
  pending = 0U;
  return 1U;
}

uint32_t vision_task_bad_frames(void)
{
  return bad_frames;
}
