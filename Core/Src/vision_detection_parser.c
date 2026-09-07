#include "vision_detection_parser.h"

#include <stddef.h>

static uint8_t parse_integer(const char **cursor,
                             char delimiter,
                             int32_t minimum,
                             int32_t maximum,
                             int32_t *value)
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
  if (digits == 0U || result < minimum || result > maximum ||
      *text != delimiter)
  {
    return 0U;
  }
  *cursor = delimiter != '\0' ? text + 1 : text;
  *value = result;
  return 1U;
}

static uint8_t parse_frame(const char *frame, VisionDetection *output)
{
  const char *cursor = frame;
  int32_t class_id, score, center_x, center_y;

  if (cursor[0] != 'D' || cursor[1] != ',')
  {
    return 0U;
  }
  cursor += 2;
  if (!parse_integer(&cursor, ',', -1L, 4L, &class_id) ||
      !parse_integer(&cursor, ',', 0L, 100L, &score) ||
      !parse_integer(&cursor, ',', 0L, 319L, &center_x) ||
      !parse_integer(&cursor, '\0', 0L, 239L, &center_y))
  {
    return 0U;
  }
  if (class_id < 0L && (score != 0L || center_x != 0L || center_y != 0L))
  {
    return 0U;
  }

  output->class_id = (int8_t)class_id;
  output->score = (uint8_t)score;
  output->center_x = (uint16_t)center_x;
  output->center_y = (uint16_t)center_y;
  output->received_ms = 0U;
  output->sequence = 0U;
  return 1U;
}

void VisionDetectionParser_Init(VisionDetectionParser *parser)
{
  if (parser == NULL)
  {
    return;
  }
  parser->length = 0U;
  parser->collecting = 0U;
}

VisionParseResult VisionDetectionParser_Consume(VisionDetectionParser *parser,
                                                uint8_t byte,
                                                VisionDetection *output)
{
  if (parser == NULL || output == NULL)
  {
    return VISION_PARSE_BAD_FRAME;
  }
  if (byte == '$')
  {
    parser->length = 0U;
    parser->collecting = 1U;
    return VISION_PARSE_COLLECTING;
  }
  if (parser->collecting == 0U)
  {
    return VISION_PARSE_IGNORED;
  }
  if (byte == '#')
  {
    parser->frame[parser->length] = '\0';
    parser->collecting = 0U;
    if (parse_frame(parser->frame, output) != 0U)
    {
      return VISION_PARSE_FRAME;
    }
    parser->length = 0U;
    return VISION_PARSE_BAD_FRAME;
  }
  if (byte == '\r' || byte == '\n' || byte < 0x20U || byte > 0x7EU ||
      parser->length >= VISION_DETECTION_FRAME_BUFFER_SIZE - 1U)
  {
    parser->length = 0U;
    parser->collecting = 0U;
    return VISION_PARSE_BAD_FRAME;
  }
  parser->frame[parser->length++] = (char)byte;
  return VISION_PARSE_COLLECTING;
}
