#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "sign_route.h"
#include "sign_route_config.h"

static uint32_t now, seq, lc, rc;
static SignRouteCommand cmd;
static SignRouteStatus status;

static void step(uint8_t mask, int32_t left_mm, int32_t right_mm)
{
  lc += (uint32_t)(left_mm * 7); rc += (uint32_t)(right_mm * 7);
  now += 10U;
  SignRoute_UpdateEncoders((int32_t)lc,(int32_t)lc,(int32_t)rc,(int32_t)rc);
  SignRoute_Step(mask,now,&cmd);
  SignRoute_GetStatus(now,&status);
  /* Navigation itself never counter-rotates, including failures. */
  assert(cmd.left_pwm >= 0 && cmd.right_pwm >= 0);
}
static void init(uint32_t origin)
{
  now=origin; seq=0; lc=rc=0x7FFFFFD0U; /* cross signed encoder rollover */
  SignRoute_Reset();
  SignRoute_UpdateEncoders((int32_t)lc,(int32_t)lc,(int32_t)rc,(int32_t)rc);
}
static void observe(int8_t cls)
{
  VisionDetection d={0};
  d.class_id=cls; d.score=cls<0?0U:70U;
  d.center_x=cls<0?0U:160U; d.center_y=cls<0?0U:120U;
  d.received_ms=now; d.sequence=++seq;
  SignRoute_ObserveDetection(&d);
}
static void confirm(int8_t side)
{
  unsigned i;
  for(i=0;i<3;++i) { observe(side<0?0:1); step(6,0,0); }
}
static void enter_probe(void)
{
  unsigned i;
  for(i=0;i<4;++i) step(15,1,1);
  assert(status.state==SIGN_ROUTE_PROBE && cmd.active);
  assert(cmd.left_pwm==cmd.right_pwm);
}
static void leave_probe(uint8_t mask)
{
  unsigned i;
  for(i=0;i<20 && status.state==SIGN_ROUTE_PROBE;++i) step(mask,2,2);
}
static void finish_select(int8_t side, SignRouteState next)
{
  unsigned i;
  SignRouteState selecting=status.state;
  for(i=0;i<20;++i) step(side<0?8U:1U,side<0?0:4,side<0?4:0);
  for(i=0;i<10 && status.state==selecting;++i)
    step(6U,side<0?0:4,side<0?4:0);
  assert(status.state==next);
}
static void start_arc(int8_t side)
{
  confirm(side); enter_probe(); leave_probe(0U);
  assert(status.state==SIGN_ROUTE_SELECTING);
  assert(side<0 ? cmd.left_pwm==0 : cmd.right_pwm==0);
  finish_select(side,SIGN_ROUTE_ARC);
}
static void half_arc(int8_t side)
{
  unsigned i;
  /* Curve toward the circle; an early outside hit must not be an exit. */
  for(i=0;i<4;++i) step(side<0?12U:3U,1,1);
  assert(status.state==SIGN_ROUTE_ARC);
  for(i=0;i<200;++i)
  {
    if(i%10==0) observe(side<0?1:0); /* contrary sign cannot reroute the arc */
    step(6U,side<0?4:2,side<0?2:4);
    assert(status.state==SIGN_ROUTE_ARC && !cmd.active);
  }
  assert(status.direction==side);
  assert(status.travel_mm>500 && status.yaw_mdeg*side>150000);
}
static void test_exit(int8_t side, uint32_t origin)
{
  unsigned i;
  init(origin); start_arc(side); half_arc(side);
  for(i=0;i<10;++i) step(side<0?3U:12U,1,1); /* inside is not the outlet */
  assert(status.state==SIGN_ROUTE_ARC);
  step(side<0?12U:3U,1,1);
  now+=100U; /* one old observation must not confirm after a scheduling gap */
  step(side<0?12U:3U,1,1);
  assert(status.state==SIGN_ROUTE_ARC);
  for(i=0;i<4;++i) step(side<0?12U:3U,1,1);
  assert(status.state==SIGN_ROUTE_EXIT_SELECT && cmd.active);
  assert(side<0 ? cmd.left_pwm==0 : cmd.right_pwm==0);
  finish_select(side,SIGN_ROUTE_EXIT_CLEAR);
  for(i=0;i<50 && status.state!=SIGN_ROUTE_LOCKED;++i) step(6U,2,2);
  assert(status.state==SIGN_ROUTE_LOCKED && !cmd.active);
  /* No-target followed by silence is not sufficient for rearming. */
  observe(-1);
  for(i=0;i<180;++i) step(6U,0,0);
  assert(status.state==SIGN_ROUTE_LOCKED);
  for(i=0;i<100;++i) { if(i%10==0) observe(-1); step(6U,0,0); }
  assert(status.state==SIGN_ROUTE_IDLE);
  printf("PASS: %s semicircle -> outward branch -> exit -> fresh rearm (origin %lu)\n",
      side<0?"left":"right",(unsigned long)origin);
}
static void test_crossbar_and_missing_sign(void)
{
  unsigned i;
  init(100); confirm(1); enter_probe(); leave_probe(6U);
  assert(status.state==SIGN_ROUTE_ARMED); /* crossbar must not start turn */
  init(100); enter_probe(); leave_probe(6U);
  assert(status.state==SIGN_ROUTE_IDLE); /* also crosses with no K210 */
  init(100); enter_probe(); leave_probe(0U);
  assert(status.state==SIGN_ROUTE_WAIT_SIGN && cmd.left_pwm==0 && cmd.right_pwm==0);
  for(i=0;i<100;++i) { observe(3); step(0,0,0); }
  assert(status.state==SIGN_ROUTE_WAIT_SIGN);
  confirm(-1); /* fresh arrows only */
  assert(status.state==SIGN_ROUTE_SELECTING && cmd.right_pwm>0 && cmd.left_pwm==0);
  SignRoute_Reset(); step(6,0,0);
  assert(status.state==SIGN_ROUTE_IDLE && !cmd.active);
  puts("PASS: transverse mark probe, branch waiting, ignored digits, reset");
}
static void test_bounds(void)
{
  unsigned i;
  init(100); start_arc(1);
  for(i=0;i<400 && status.state!=SIGN_ROUTE_FAULT;++i) step(6,2,4);
  assert(status.state==SIGN_ROUTE_FAULT && status.fault==3U);
  assert(cmd.active && cmd.left_pwm==0 && cmd.right_pwm==0);
  confirm(1); step(15,0,0);
  assert(status.state==SIGN_ROUTE_FAULT); /* observations cannot restart it */
  init(100); start_arc(-1);
  for(i=0;i<65;++i) step(0,0,0);
  assert(status.state==SIGN_ROUTE_FAULT && status.fault==4U);
  init(100); confirm(1); enter_probe(); leave_probe(0);
  for(i=0;i<260;++i) step(1,0,0);
  assert(status.state==SIGN_ROUTE_FAULT && status.fault==2U);
  init(100); confirm(1); enter_probe();
  for(i=0;i<190;++i) step(15,0,0);
  assert(status.state==SIGN_ROUTE_FAULT && status.fault==1U);
  init(100); start_arc(1); half_arc(1);
  for(i=0;i<4;++i) step(3,1,1);
  finish_select(1,SIGN_ROUTE_EXIT_CLEAR);
  for(i=0;i<15;++i) step(0,1,1);
  assert(status.state==SIGN_ROUTE_FAULT && status.fault==5U);
  puts("PASS: missed exit, no encoder progress, lost line and probe timeout latch STOP");
}
int main(void)
{
  test_exit(-1,100U); test_exit(1,100U);
  test_exit(-1,UINT32_MAX-1000U); test_exit(1,UINT32_MAX-1000U);
  test_crossbar_and_missing_sign(); test_bounds();
  return 0;
}
