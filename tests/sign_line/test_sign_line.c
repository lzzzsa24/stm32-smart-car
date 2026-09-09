#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sign_route.h"
#include "sign_slowdown.h"
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

static void test_slowdown(void)
{
  VisionDetectionParser parser;
  VisionDetection frame;
  SignRouteStatus status;
  uint32_t seq = 0U;
  unsigned i;
  const char *frames[] = {"$D,0,20,160,120#", "$D,1,20,160,120#",
      "$D,2,20,160,120#", "$D,3,20,160,120#", "$D,4,20,160,120#"};
  VisionDetectionParser_Init(&parser);
  for (i = 0U; i < 5U; ++i)
  {
    SignSlowdown_Reset(); SignRoute_Reset();
    CHECK(feed(&parser, frames[i], &frame) == VISION_PARSE_FRAME);
    frame.received_ms = 100U; frame.sequence = ++seq;
    SignSlowdown_ObserveDetection(&frame, 100U);
    SignRoute_ObserveDetection(&frame);
    SignRoute_GetStatus(100U, &status);
    CHECK(status.state == SIGN_ROUTE_IDLE); /* one frame slows, never routes */
    CHECK(SignSlowdown_Reasons(100U) == SIGN_SLOWDOWN_VISION);
    SignSlowdown_ObserveDetection(&frame, 1400U); /* repeated read cannot renew */
    CHECK(SignSlowdown_Reasons(1599U) == SIGN_SLOWDOWN_VISION);
    CHECK(SignSlowdown_Reasons(1600U) == 0U);
  }
  SignSlowdown_Reset();
  SignSlowdown_ObserveBlack(0U); /* zero timestamp is valid */
  CHECK(SignSlowdown_Reasons(0U) == SIGN_SLOWDOWN_BLACK);
  CHECK(feed(&parser, "$D,-1,0,0,0#", &frame) == VISION_PARSE_FRAME);
  frame.sequence = ++seq; frame.received_ms = 1499U;
  SignSlowdown_ObserveDetection(&frame, 1499U);
  CHECK(SignSlowdown_Reasons(1500U) == 0U); /* none does not prolong */
  CHECK(feed(&parser, "$D,0,101,10,10#", &frame) == VISION_PARSE_BAD_FRAME);
  CHECK(SignSlowdown_Reasons(1501U) == 0U);
  CHECK(feed(&parser, frames[0], &frame) == VISION_PARSE_FRAME);
  frame.sequence = ++seq; frame.received_ms = 100U;
  SignSlowdown_ObserveDetection(&frame, 451U);
  CHECK(SignSlowdown_Reasons(451U) == 0U); /* stale frame */
  frame.sequence = ++seq; frame.received_ms = 1000U;
  SignSlowdown_ObserveDetection(&frame, 1000U);
  SignSlowdown_ObserveBlack(2000U);
  CHECK(SignSlowdown_Reasons(2499U) == 3U);
  CHECK(SignSlowdown_Reasons(2500U) == SIGN_SLOWDOWN_BLACK);
  CHECK(SignSlowdown_Reasons(3500U) == 0U);
  SignSlowdown_ObserveBlack(UINT32_MAX - 100U);
  CHECK(SignSlowdown_Reasons(1398U) == SIGN_SLOWDOWN_BLACK);
  CHECK(SignSlowdown_Reasons(1399U) == 0U);
  frame.sequence = UINT32_MAX; frame.received_ms = UINT32_MAX - 50U;
  SignSlowdown_ObserveDetection(&frame, UINT32_MAX - 50U);
  frame.sequence = 0U; frame.received_ms = 20U;
  SignSlowdown_ObserveDetection(&frame, 20U);
  CHECK(SignSlowdown_Reasons(1519U) == SIGN_SLOWDOWN_VISION);
  CHECK(SignSlowdown_Reasons(1520U) == 0U);
  SignSlowdown_ObserveBlack(2000U);
  SignSlowdown_Reset();
  CHECK(SignSlowdown_Reasons(2001U) == 0U);
  CHECK(SignSlowdown_TargetLimit(3U, -2700, 2700) == 0L);
  CHECK(SignSlowdown_TargetLimit(3U, 2700, -2700) == 0L);
  CHECK(SignSlowdown_TargetLimit(3U, 2400, 2400) == 500L);
  CHECK(SignSlowdown_TargetLimit(3U, 0, 2200) == 500L);
  CHECK(SignSlowdown_TargetLimit(SIGN_SLOWDOWN_VISION, 800, 2300) == 500L);
  CHECK(SignSlowdown_TargetLimit(0U, 2400, 2400) == 1200L);
  SignRoute_Reset();
  SignSlowdown_Reset();
  CHECK(feed(&parser, "$D,0,15,160,120#", &frame) == VISION_PARSE_FRAME);
  frame.sequence = ++seq; frame.received_ms = 4000U;
  SignRoute_ObserveDetection(&frame);
  SignSlowdown_ObserveDetection(&frame, 4000U);
  CHECK(SignSlowdown_TargetLimit(SignSlowdown_Reasons(4000U),2300,2300)==500L);
  CHECK(SignSlowdown_TargetLimit(SignSlowdown_Reasons(5499U),2300,2300)==500L);
  CHECK(SignSlowdown_TargetLimit(SignSlowdown_Reasons(5500U),2300,2300)==1200L);
  {
    SignRouteStatus weak_status;
    SignRoute_GetStatus(4000U,&weak_status);
    CHECK(weak_status.direction==0);
  }
  puts("PASS: one-frame slowdown, no-target/stale rejection, independent holds, reset and wrap");
}

static void test_slow_profile(void)
{
  SimpleLineController c;
  unsigned i;
  SimpleLine_Init(&c); SimpleLine_Start(&c);
  for(i=0;i<1000;++i)
  {
    SimpleLine_StepSlow(&c, i%2 ? 2U:4U);
    CHECK(c.left_pwm>=2200 && c.right_pwm>=2200);
    CHECK(abs(c.left_pwm-c.right_pwm)==100);
    CHECK(SignSlowdown_TargetLimit(0,c.left_pwm,c.right_pwm)==1200);
    CHECK(SignSlowdown_TargetLimit(SIGN_SLOWDOWN_BLACK,c.left_pwm,c.right_pwm)==700);
  }
  for(i=1;i<16;++i)
  {
    SimpleLine_StepSlow(&c,0);
    SimpleLine_StepSlow(&c,(uint8_t)i);
    CHECK(c.left_pwm>0 && c.right_pwm>0);
    CHECK(c.left_pwm<=2300 && c.right_pwm<=2300);
  }
  SimpleLine_StepSlow(&c,1); SimpleLine_StepSlow(&c,0);
  CHECK(c.left_pwm==2700 && c.right_pwm==-2700);
  for(i=0;i<1000;++i)
  {
    int32_t left, right;
    SimpleLine_StepSlow(&c, i%2 ? 1U : 8U);
    left=SignSlowdown_ForwardCps(c.left_pwm);
    right=SignSlowdown_ForwardCps(c.right_pwm);
    CHECK(left>0 && right>0);
    CHECK(i%2 ? left==1200 && right<left/2 : right==1200 && left<right/2);
  }
  CHECK(SignSlowdown_ForwardCps(0)==0);
  CHECK(SignSlowdown_ForwardCps(2200)<SignSlowdown_ForwardCps(2300));
  SimpleLine_Stop(&c); SimpleLine_StepSlow(&c,6);
  CHECK(c.left_pwm==0 && c.right_pwm==0);
  puts("PASS: gentle alternating inner correction, steady cap, all visible masks forward, search and STOP");
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
static void test_observation_pause(void)
{
  VisionDetection f={0}; unsigned i;
  SignRouteStatus r;
  SignSlowdown_Reset(); SignRoute_Reset();
  SignSlowdown_AllowPause(1);
  f.class_id=0; f.score=25; f.center_x=160; f.center_y=120;
  for(i=0;i<30;++i)
  {
    f.sequence=i+1; f.received_ms=100+i*100;
    SignSlowdown_ObserveDetection(&f,f.received_ms);
    SignRoute_ObserveDetection(&f);
    CHECK(SignSlowdown_Paused(f.received_ms)==(i<20));
  }
  SignRoute_GetStatus(3000,&r); CHECK(r.direction==-1);
  /* Continuous/noisy detections cannot renew or rearm the deadline. */
  f.class_id=1; f.sequence++; f.received_ms=3100;
  SignSlowdown_ObserveDetection(&f,3100); CHECK(!SignSlowdown_Paused(3100));
  f.class_id=-1;
  for(i=0;i<=15;++i)
  { f.sequence++; f.received_ms=3200+i*100; SignSlowdown_ObserveDetection(&f,f.received_ms); }
  f.class_id=1; f.sequence++; f.received_ms=4800;
  SignSlowdown_ObserveDetection(&f,4800); CHECK(SignSlowdown_Paused(4800));
  CHECK(SignSlowdown_Paused(6799)); CHECK(!SignSlowdown_Paused(6800));
  SignSlowdown_Reset(); CHECK(!SignSlowdown_Paused(4801));
  SignSlowdown_AllowPause(0); f.sequence++; f.received_ms=5000;
  SignSlowdown_ObserveDetection(&f,5000); CHECK(!SignSlowdown_Paused(5000));
  SignSlowdown_Reset(); SignSlowdown_AllowPause(1);
  f.sequence++; f.received_ms=UINT32_MAX-999U;
  SignSlowdown_ObserveDetection(&f,f.received_ms);
  CHECK(SignSlowdown_Paused(999)); CHECK(!SignSlowdown_Paused(1000));
  puts("PASS: fixed 2s observation, votes while stopped, no renewal, fresh rearm, reset and wrap");
}
int main(void)
{
  test_parser();
  test_simple_line();
  test_sign_route();
  test_slowdown();
  test_slow_profile();
  test_entry_direction();
  test_search_sector();
  test_observation_pause();
  return 0;
}
