#include "vision_line_v4_parser.h"

#include <stddef.h>

static uint8_t parse_integer(const char **cursor,
                             char delimiter,
                             int32_t minimum,
                             int32_t maximum,
                             int32_t *value)
{
  const char *text = *cursor;
  int32_t result = 0L;
  int32_t sign = 1L;
  uint8_t digits = 0U;

  if (*text == '-')
  {
    sign = -1L;
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
  if (digits == 0U || result < minimum || result > maximum ||
      *text != delimiter)
  {
    return 0U;
  }
  *cursor = delimiter == '\0' ? text : text + 1;
  *value = result;
  return 1U;
}

static uint8_t parse_frame(const char *frame, VisionLineV4Reading *output)
{
  const char *cursor = frame;
  int32_t status;
  int32_t offset;
  int32_t angle;
  int32_t bottom;
  int32_t obstacle;
  int32_t obstacle_bottom;
  int32_t obstacle_left;
  int32_t obstacle_right;

  if (!parse_integer(&cursor, ',', 0L, 1L, &status) ||
      !parse_integer(&cursor, ',', -160L, 160L, &offset) ||
      !parse_integer(&cursor, ',', -1L, 180L, &angle) ||
      !parse_integer(&cursor, ',', -1L, 319L, &bottom) ||
      !parse_integer(&cursor, ',', 0L, 1L, &obstacle) ||
      !parse_integer(&cursor, ',', 0L, 240L, &obstacle_bottom) ||
      !parse_integer(&cursor, ',', 0L, 319L, &obstacle_left) ||
      !parse_integer(&cursor, '\0', 0L, 320L, &obstacle_right))
  {
    return 0U;
  }

  if ((status == 0L && (offset != -1L || bottom != -1L)) ||
      (status == 1L && (angle < 0L || bottom < 0L)) ||
      (obstacle == 0L &&
       (obstacle_bottom != 0L || obstacle_left != 0L ||
        obstacle_right != 0L)) ||
      (obstacle == 1L &&
       (obstacle_bottom == 0L || obstacle_right <= obstacle_left)))
  {
    return 0U;
  }

  output->frame_valid = 1U;
  output->line_found = (uint8_t)status;
  output->offset = (int16_t)offset;
  output->angle = (int16_t)angle;
  output->bottom = (int16_t)bottom;
  output->obstacle_found = (uint8_t)obstacle;
  output->obstacle_bottom = (uint16_t)obstacle_bottom;
  output->obstacle_left = (uint16_t)obstacle_left;
  output->obstacle_right = (uint16_t)obstacle_right;
  output->received_ms = 0U;
  output->sequence = 0U;
  return 1U;
}

void VisionLineV4Parser_Init(VisionLineV4Parser *parser)
{
  if (parser == NULL)
  {
    return;
  }
  parser->length = 0U;
  parser->collecting = 0U;
}

VisionLineV4ParseResult VisionLineV4Parser_Consume(
    VisionLineV4Parser *parser,
    uint8_t byte,
    VisionLineV4Reading *output)
{
  if (parser == NULL || output == NULL)
  {
    return VISION_LINE_V4_PARSE_BAD_FRAME;
  }
  if (byte == '$')
  {
    parser->length = 0U;
    parser->collecting = 1U;
    return VISION_LINE_V4_PARSE_COLLECTING;
  }
  if (parser->collecting == 0U)
  {
    return VISION_LINE_V4_PARSE_IGNORED;
  }
  if (byte == '#')
  {
    parser->frame[parser->length] = '\0';
    parser->collecting = 0U;
    if (parse_frame(parser->frame, output) != 0U)
    {
      return VISION_LINE_V4_PARSE_FRAME;
    }
    parser->length = 0U;
    return VISION_LINE_V4_PARSE_BAD_FRAME;
  }
  if (byte == '\r' || byte == '\n' || byte < 0x20U || byte > 0x7EU ||
      parser->length >= VISION_LINE_V4_FRAME_BUFFER_SIZE - 1U)
  {
    parser->length = 0U;
    parser->collecting = 0U;
    return VISION_LINE_V4_PARSE_BAD_FRAME;
  }
  parser->frame[parser->length++] = (char)byte;
  return VISION_LINE_V4_PARSE_COLLECTING;
}
