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
  SignRouteStatus status;
  SignRoute_Init();
  confirm_direction(0, 1U);
  SignRoute_GetStatus(250U, &status);
  CHECK(status.state == SIGN_ROUTE_ARMED && status.direction == -1);
  SignRoute_Reset();
  confirm_direction(1, 1U);
  SignRoute_GetStatus(250U, &status);
  CHECK(status.state == SIGN_ROUTE_ARMED && status.direction == 1);
  puts("PASS: three-vote left/right confirmation");
}


static void test_entry_direction(void)
{
  int side; unsigned i;
  for(side=-1;side<=1;side+=2)
  {
    SimpleLineController c, old;
    SignRouteStatus r={0}; SignRouteCommand cmd={0};
    SimpleLine_Init(&c); SimpleLine_Start(&c);
    r.state=SIGN_ROUTE_PROBE; r.direction=(int8_t)side; cmd.just_started=1;
    SimpleLine_StepRoute(&c,15,&r,&cmd);
    CHECK(c.last_direction==side);
    cmd.just_started=0;
    SimpleLine_StepRoute(&c,side<0?2:4,&r,&cmd);
    CHECK(c.last_direction==-side);
    old=c; /* reproduce deployed continuous PROBE override */
    SimpleLine_SetDirection(&old,(int8_t)side); SimpleLine_StepSlow(&old,0);
    CHECK(side<0 ? old.left_pwm<0 : old.left_pwm>0);
    for(i=0;i<500;++i)
    {
      SimpleLine_StepRoute(&c,0,&r,&cmd);
      CHECK(side<0 ? c.left_pwm>0 && c.right_pwm<0 : c.left_pwm<0 && c.right_pwm>0);
    }
    r.state=SIGN_ROUTE_ARC; cmd.just_finished=1;
    SimpleLine_StepRoute(&c,side<0?8:1,&r,&cmd);
    CHECK(c.last_direction==side); /* live contact wins on capture cycle */
    cmd.just_finished=0; SimpleLine_StepRoute(&c,0,&r,&cmd);
    CHECK(c.last_direction==side);
    r.state=SIGN_ROUTE_PROBE; r.direction=0;
    SimpleLine_StepRoute(&c,6,&r,&cmd);
    r.direction=(int8_t)-side;
    SimpleLine_StepRoute(&c,0,&r,&cmd);
    CHECK(c.last_direction==-side); /* late confirmation is seeded once */
    SimpleLine_Stop(&c); SimpleLine_StepRoute(&c,8,&r,&cmd);
    CHECK(c.left_pwm==0 && c.right_pwm==0);
  }
  puts("PASS: deployed wrong-way PROBE loss reproduced; one-shot hint preserves live curve/search and STOP");
}
static void test_search_sector(void)
{
  SimpleLineController c;
  SignRouteStatus r={0}; SignRouteCommand cmd={0};
  int64_t angle=10000000; unsigned i;
  SimpleLine_Init(&c); SimpleLine_Start(&c);
  r.state=SIGN_ROUTE_PROBE; r.direction=-1;
  SimpleLine_UpdateYaw(&c,angle,1,1);
  SimpleLine_StepRoute(&c,6,&r,&cmd);
  for(i=0;i<1000;++i)
  {
    SimpleLine_UpdateYaw(&c,angle,1,1);
    SimpleLine_StepRoute(&c,0,&r,&cmd);
    CHECK(c.left_pwm==-c.right_pwm && c.left_pwm!=0);
    angle += c.left_pwm<0 ? 2000 : -2000;
    CHECK(angle>=10000000-27000 && angle<=10000000+27000);
  }
  SimpleLine_UpdateYaw(&c,angle,0,1);
  SimpleLine_StepRoute(&c,0,&r,&cmd);
  CHECK(c.mode==SIMPLE_LINE_SEARCH && c.left_pwm==0 && c.right_pwm==0);
  SimpleLine_UpdateYaw(&c,-5000000,1,2); /* IMU reset must not use old angle origin */
  SimpleLine_StepRoute(&c,0,&r,&cmd);
  CHECK(c.line_yaw_mdeg==-5000000 && c.left_pwm!=0);
  SimpleLine_StepRoute(&c,3,&r,&cmd);
  CHECK(c.left_pwm>0 && c.right_pwm>0 && !c.sector_active);
  SimpleLine_Stop(&c); SimpleLine_UpdateYaw(&c,0,1,2);
  SimpleLine_StepRoute(&c,0,&r,&cmd);
  CHECK(c.left_pwm==0 && c.right_pwm==0);
  puts("PASS: measured-yaw sector contains repeated search, stale yaw withdrawal, generation reset and STOP");
}
int main(void)
{
  test_parser();
  test_simple_line();
  test_sign_route();
  test_entry_direction();
  test_search_sector();
  return 0;
}
