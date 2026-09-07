#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sign_route.h"
#include "simple_line_mode.h"
#include "vision_detection_parser.h"

#define CHECK(condition) do { if (!(condition)) { \
  fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); \
} } while (0)

static VisionParseResult feed(VisionDetectionParser *parser,
                              const char *text,
                              VisionDetection *detection)
{
  VisionParseResult result = VISION_PARSE_IGNORED;
  while (*text != '\0')
  {
    result = VisionDetectionParser_Consume(
        parser, (unsigned char)*text++, detection);
  }
  return result;
}

static void test_parser(void)
{
  VisionDetectionParser parser;
  VisionDetection detection;

  VisionDetectionParser_Init(&parser);
  CHECK(feed(&parser, "$D,0,52,160,120#", &detection) == VISION_PARSE_FRAME);
  CHECK(detection.class_id == 0 && detection.score == 52U);
  CHECK(detection.center_x == 160U && detection.center_y == 120U);
  CHECK(feed(&parser, "\r\n$D,-1,0,0,0#", &detection) == VISION_PARSE_FRAME);
  CHECK(detection.class_id == -1);
  CHECK(feed(&parser, "$D,-1,1,0,0#", &detection) == VISION_PARSE_BAD_FRAME);
  CHECK(feed(&parser, "$D,1,20,319,239#", &detection) == VISION_PARSE_FRAME);
  CHECK(detection.class_id == 1 && detection.score == 20U);
  CHECK(feed(&parser, "$D,1,20,320,20#", &detection) == VISION_PARSE_BAD_FRAME);
  CHECK(feed(&parser, "$D,0,30,$D,1,88,20,30#", &detection) ==
        VISION_PARSE_FRAME);
  CHECK(detection.class_id == 1 && detection.score == 88U);
  CHECK(feed(&parser, "$D,2,90,10,10,7#", &detection) ==
        VISION_PARSE_BAD_FRAME);
}

static void expect_simple(uint8_t mask,
                          SimpleLineMode mode,
                          int16_t left,
                          int16_t right)
{
  SimpleLineController controller;
  SimpleLine_Init(&controller);
  SimpleLine_Start(&controller);
  SimpleLine_Step(&controller, mask);
  CHECK(controller.mode == mode);
  CHECK(controller.left_pwm == left);
  CHECK(controller.right_pwm == right);
}

static void test_simple_line(void)
{
  SimpleLineController controller;

  expect_simple(6U, SIMPLE_LINE_TRACK, 2400, 2400);
  expect_simple(4U, SIMPLE_LINE_TRACK, 2200, 2600);
  expect_simple(2U, SIMPLE_LINE_TRACK, 2600, 2200);
  expect_simple(8U, SIMPLE_LINE_TURN, -2700, 2700);
  expect_simple(12U, SIMPLE_LINE_TURN, -2700, 2700);
  expect_simple(1U, SIMPLE_LINE_TURN, 2700, -2700);
  expect_simple(3U, SIMPLE_LINE_TURN, 2700, -2700);
  expect_simple(0U, SIMPLE_LINE_SEARCH, -2700, 2700);
  expect_simple(15U, SIMPLE_LINE_WIDE, 2200, 2200);
  expect_simple(5U, SIMPLE_LINE_WIDE, 2200, 2200);

  SimpleLine_Init(&controller);
  SimpleLine_Start(&controller);
  SimpleLine_Step(&controller, 1U);
  SimpleLine_Step(&controller, 0U);
  SimpleLine_Step(&controller, 0U);
  CHECK(controller.mode == SIMPLE_LINE_SEARCH);
  CHECK(controller.left_pwm == 2700 && controller.right_pwm == -2700);
  SimpleLine_Start(&controller);
  CHECK(controller.last_direction == 1);
}

static void observe(int8_t class_id,
                    uint8_t score,
                    uint16_t center_x,
                    uint16_t center_y,
                    uint32_t time_ms,
                    uint32_t sequence)
{
  VisionDetection detection;
  detection.class_id = class_id;
  detection.score = score;
  detection.center_x = center_x;
  detection.center_y = center_y;
  detection.received_ms = time_ms;
  detection.sequence = sequence;
  SignRoute_ObserveDetection(&detection);
}

static void confirm_direction(int8_t class_id, uint32_t start_sequence)
{
  observe(class_id, 70U, 150U, 100U, 100U, start_sequence);
  observe(-1, 0U, 0U, 0U, 150U, start_sequence + 1U);
  observe(class_id, 72U, 155U, 102U, 200U, start_sequence + 2U);
  observe(class_id, 74U, 158U, 104U, 250U, start_sequence + 3U);
}

static void test_sign_route(void)
{
  SignRouteCommand command;
  SignRouteStatus status;

  SignRoute_Init();
  confirm_direction(0, 1U);
  SignRoute_GetStatus(250U, &status);
  CHECK(status.state == SIGN_ROUTE_ARMED && status.direction == -1);
  SignRoute_Step(15U, 250U, &command);
  CHECK(command.active == 0U);
  SignRoute_Step(15U, 271U, &command);
  CHECK(command.active != 0U && command.just_started != 0U);
  CHECK(command.left_pwm == -2700 && command.right_pwm == 2700);
  SignRoute_Step(8U, 360U, &command);
  CHECK(command.active != 0U);
  SignRoute_Step(4U, 400U, &command);
  CHECK(command.active != 0U);
  SignRoute_Step(4U, 421U, &command);
  CHECK(command.active == 0U && command.just_finished != 0U);
  SignRoute_GetStatus(421U, &status);
  CHECK(status.state == SIGN_ROUTE_LOCKED);

  observe(0, 90U, 160U, 110U, 500U, 5U);
  observe(0, 90U, 160U, 110U, 600U, 6U);
  observe(0, 90U, 160U, 110U, 700U, 7U);
  SignRoute_GetStatus(700U, &status);
  CHECK(status.state == SIGN_ROUTE_LOCKED);
  observe(-1, 0U, 0U, 0U, 800U, 8U);
  observe(-1, 0U, 0U, 0U, 1000U, 9U);
  SignRoute_Step(6U, 1922U, &command);
  SignRoute_GetStatus(1922U, &status);
  CHECK(status.state == SIGN_ROUTE_IDLE);

  SignRoute_Reset();
  observe(2, 99U, 100U, 100U, 100U, 1U);
  observe(2, 99U, 100U, 100U, 200U, 2U);
  observe(2, 99U, 100U, 100U, 300U, 3U);
  SignRoute_GetStatus(300U, &status);
  CHECK(status.state == SIGN_ROUTE_IDLE);

  SignRoute_Reset();
  confirm_direction(1, 1U);
  SignRoute_Step(7U, 250U, &command);
  SignRoute_Step(7U, 271U, &command);
  CHECK(command.active != 0U && command.left_pwm == 2700 &&
        command.right_pwm == -2700);
  SignRoute_Step(6U, 5300U, &command);
  CHECK(command.active != 0U); /* selection keeps searching until capture */

  SignRoute_Reset();
  confirm_direction(0, 1U);
  SignRoute_Step(6U, 5300U, &command);
  SignRoute_GetStatus(5300U, &status);
  CHECK(status.state == SIGN_ROUTE_IDLE); /* unused reservation expires */

  puts("PASS: strict $D parser, exact SL2 table and confirmed sign routing");
}

int main(void)
{
  test_parser();
  test_simple_line();
  test_sign_route();
  return 0;
}
