#ifndef VISION_LINE_V4_PARSER_H
#define VISION_LINE_V4_PARSER_H

#include <stdint.h>

#include "vision_line_v4.h"

#define VISION_LINE_V4_FRAME_BUFFER_SIZE 80U

typedef enum
{
  VISION_LINE_V4_PARSE_IGNORED = 0,
  VISION_LINE_V4_PARSE_COLLECTING,
  VISION_LINE_V4_PARSE_FRAME,
  VISION_LINE_V4_PARSE_BAD_FRAME
} VisionLineV4ParseResult;

typedef struct
{
  char frame[VISION_LINE_V4_FRAME_BUFFER_SIZE];
  uint8_t length;
  uint8_t collecting;
} VisionLineV4Parser;

void VisionLineV4Parser_Init(VisionLineV4Parser *parser);
VisionLineV4ParseResult VisionLineV4Parser_Consume(
    VisionLineV4Parser *parser,
    uint8_t byte,
    VisionLineV4Reading *output);

#endif
