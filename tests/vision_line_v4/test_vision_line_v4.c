#include <stdio.h>
#include <stdlib.h>

#include "vision_line_v4_control.h"
#include "vision_line_v4_parser.h"

#define CHECK(condition) do { if (!(condition)) { \
  fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); \
} } while (0)

static VisionLineV4ParseResult feed(VisionLineV4Parser *parser,
                                    const char *text,
                                    VisionLineV4Reading *reading)
{
  VisionLineV4ParseResult result = VISION_LINE_V4_PARSE_IGNORED;

  while (*text != '\0')
  {
    result = VisionLineV4Parser_Consume(
        parser, (uint8_t)*text++, reading);
  }
  return result;
}

static void test_parser(void)
{
  VisionLineV4Parser parser;
  VisionLineV4Reading reading;

  VisionLineV4Parser_Init(&parser);
  CHECK(feed(&parser, "$1,0,90,160,0,0,0,0#", &reading) ==
        VISION_LINE_V4_PARSE_FRAME);
  CHECK(reading.line_found == 1U && reading.offset == 0);
  CHECK(reading.angle == 90 && reading.bottom == 160);
  CHECK(reading.obstacle_found == 0U);

  CHECK(feed(&parser, "$1,-60,120,100,1,150,80,240#", &reading) ==
        VISION_LINE_V4_PARSE_FRAME);
  CHECK(reading.offset == -60 && reading.angle == 120);
  CHECK(reading.obstacle_found == 1U &&
        reading.obstacle_bottom == 150U &&
        reading.obstacle_left == 80U && reading.obstacle_right == 240U);

  CHECK(feed(&parser, "$0,-1,5,-1,0,0,0,0#", &reading) ==
        VISION_LINE_V4_PARSE_FRAME);
  CHECK(reading.line_found == 0U && reading.angle == 5);
  CHECK(feed(&parser, "$0,0,5,-1,0,0,0,0#", &reading) ==
        VISION_LINE_V4_PARSE_BAD_FRAME);
  CHECK(feed(&parser, "$1,0,-1,160,0,0,0,0#", &reading) ==
        VISION_LINE_V4_PARSE_BAD_FRAME);
  CHECK(feed(&parser, "$1,0,90,160,0,1,0,0#", &reading) ==
        VISION_LINE_V4_PARSE_BAD_FRAME);
  CHECK(feed(&parser, "$1,0,90,160,1,150,200,100#", &reading) ==
        VISION_LINE_V4_PARSE_BAD_FRAME);
  CHECK(feed(&parser, "$D,0,80,160,120#", &reading) ==
        VISION_LINE_V4_PARSE_BAD_FRAME);
}

static VisionLineV4Reading make_reading(uint32_t sequence,
                                        uint32_t time_ms,
                                        uint8_t found,
                                        int16_t offset,
                                        int16_t angle,
                                        int16_t bottom)
{
  VisionLineV4Reading reading = {0};

  reading.frame_valid = 1U;
  reading.line_found = found;
  reading.offset = offset;
  reading.angle = angle;
  reading.bottom = bottom;
  reading.received_ms = time_ms;
  reading.sequence = sequence;
  return reading;
}

static void test_control(void)
{
  VisionLineV4Reading reading = {0};
  VisionLineV4Command command;

  VisionLineV4Control_Init();
  VisionLineV4Control_Step(&reading, 0U, &command);
  CHECK(command.state == VISION_LINE_V4_WAITING);
  CHECK(command.left_pwm == 0 && command.right_pwm == 0);

  reading = make_reading(1U, 100U, 1U, 0, 90, 160);
  VisionLineV4Control_Step(&reading, 100U, &command);
  CHECK(command.state == VISION_LINE_V4_FOLLOW);
  CHECK(command.left_pwm == 2000 && command.right_pwm == 2000);

  reading = make_reading(2U, 150U, 1U, -40, 90, 120);
  VisionLineV4Control_Step(&reading, 150U, &command);
  CHECK(command.state == VISION_LINE_V4_FOLLOW);
  CHECK(command.left_pwm < command.right_pwm);
  CHECK(command.turn_direction == -1);

  reading = make_reading(3U, 200U, 1U, -40, 50, 110);
  VisionLineV4Control_Step(&reading, 200U, &command);
  CHECK(command.state == VISION_LINE_V4_CURVE);
  CHECK(command.left_pwm < command.right_pwm);

  reading = make_reading(4U, 250U, 1U, 0, 5, 100);
  VisionLineV4Control_Step(&reading, 250U, &command);
  CHECK(command.state == VISION_LINE_V4_SHARP_TURN);
  CHECK(command.left_pwm == -1800 && command.right_pwm == 1800);

  reading = make_reading(5U, 300U, 0U, -1, 5, -1);
  VisionLineV4Control_Step(&reading, 300U, &command);
  CHECK(command.state == VISION_LINE_V4_LOST_SEARCH);
  CHECK(command.left_pwm < 0 && command.right_pwm > 0);

  reading = make_reading(6U, 701U, 0U, -1, -1, -1);
  VisionLineV4Control_Step(&reading, 701U, &command);
  CHECK(command.state == VISION_LINE_V4_LOST_SEARCH);
  CHECK(command.left_pwm < 0 && command.right_pwm > 0);

  VisionLineV4Control_Step(&reading, 852U, &command);
  CHECK(command.state == VISION_LINE_V4_LINK_STOP);

  reading = make_reading(7U, 900U, 1U, -20, 90, 140);
  VisionLineV4Control_Step(&reading, 900U, &command);
  CHECK(command.state == VISION_LINE_V4_LOST_SEARCH);
  reading = make_reading(8U, 950U, 1U, -20, 90, 140);
  VisionLineV4Control_Step(&reading, 950U, &command);
  CHECK(command.state == VISION_LINE_V4_FOLLOW);

  VisionLineV4Control_Init();
  reading = make_reading(1U, 1000U, 1U, 20, 90, 180);
  reading.obstacle_found = 1U;
  reading.obstacle_bottom = 150U;
  reading.obstacle_left = 80U;
  reading.obstacle_right = 240U;
  VisionLineV4Control_Step(&reading, 1000U, &command);
  CHECK(command.state == VISION_LINE_V4_OBSTACLE_STOP);
  CHECK(command.left_pwm == 0 && command.right_pwm == 0);
}

int main(void)
{
  test_parser();
  test_control();
  puts("PASS: visual-line v4 parser and bounded control states");
  return 0;
}
