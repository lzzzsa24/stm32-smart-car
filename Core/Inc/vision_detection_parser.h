#ifndef VISION_DETECTION_PARSER_H
#define VISION_DETECTION_PARSER_H

#include <stdint.h>

#include "vision_detection.h"

#define VISION_DETECTION_FRAME_BUFFER_SIZE 48U

typedef enum
{
  VISION_PARSE_IGNORED = 0,
  VISION_PARSE_COLLECTING,
  VISION_PARSE_FRAME,
  VISION_PARSE_BAD_FRAME
} VisionParseResult;

typedef struct
{
  char frame[VISION_DETECTION_FRAME_BUFFER_SIZE];
  uint8_t length;
  uint8_t collecting;
} VisionDetectionParser;

void VisionDetectionParser_Init(VisionDetectionParser *parser);
VisionParseResult VisionDetectionParser_Consume(VisionDetectionParser *parser,
                                                uint8_t byte,
                                                VisionDetection *output);

#endif
