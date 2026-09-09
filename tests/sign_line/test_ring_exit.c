#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "sign_route.h"
#include "sign_route_config.h"
#include "simple_line_mode.h"

static uint32_t now, seq, lc, rc;
static SignRouteCommand cmd;
static SignRouteStatus status;
static SimpleLineController line;
static int16_t motor_left, motor_right;

static void step(uint8_t mask, int32_t left_mm, int32_t right_mm)
{
  lc += (uint32_t)(left_mm * 7); rc += (uint32_t)(right_mm * 7);
  now += 10U;
  SignRoute_UpdateEncoders((int32_t)lc,(int32_t)lc,(int32_t)rc,(int32_t)rc);
  SignRoute_Step(mask,now,&cmd);
  SignRoute_GetStatus(now,&status);
  SimpleLine_StepRoute(&line,mask,&status,&cmd);
  /* Navigation itself never counter-rotates, including failures. */
  assert(cmd.left_pwm >= 0 && cmd.right_pwm >= 0);
  motor_left=cmd.active ? cmd.left_pwm : line.left_pwm;
  motor_right=cmd.active ? cmd.right_pwm : line.right_pwm;
  /* Regression: PROBE/EXIT_CLEAR must not drive blind toward the sign. */
  if(mask==0U) assert((int32_t)motor_left+motor_right<=0);
  if(mask!=0U && status.state!=SIGN_ROUTE_FAULT)
    assert(motor_left!=0 || motor_right!=0);
  if(cmd.active && (cmd.left_pwm || cmd.right_pwm))
    assert(mask!=15U && (mask & (status.direction<0 ? 8U:1U)));
}
static void init(uint32_t origin)
{
  now=origin; seq=0; lc=rc=0x7FFFFFD0U; /* cross signed encoder rollover */
  SignRoute_Reset();
  SimpleLine_Init(&line); SimpleLine_Start(&line);
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
  assert(status.state==SIGN_ROUTE_PROBE && !cmd.active);
  assert(motor_left>0 && motor_right>0);
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
  assert(status.state==SIGN_ROUTE_WAIT_SIGN && !cmd.active);
  for(i=0;i<100;++i) { observe(3); step(1,0,0); }
  assert(status.state==SIGN_ROUTE_WAIT_SIGN);
  confirm(-1); /* fresh arrows only */
  assert(status.state==SIGN_ROUTE_ARMED && !cmd.active);
  SignRoute_Reset(); step(6,0,0);
  assert(status.state==SIGN_ROUTE_IDLE && !cmd.active);
  puts("PASS: transverse mark observation, missing sign never stops live tracking, ignored digits, reset");
}
static void test_bounds(void)
{
  unsigned i;
  init(100); start_arc(1);
  for(i=0;i<400;++i) step(6,2,4);
  assert(status.state==SIGN_ROUTE_CANCELLED && status.fault==3U && !cmd.active);
  assert(motor_left>0 && motor_right>0); /* uncertain odometry cannot stop on-line */
  for(i=0;i<4;++i) step(3,0,0);
  assert(status.state==SIGN_ROUTE_CANCELLED && !cmd.active); /* no invented exit */
  init(100); start_arc(-1);
  for(i=0;i<65;++i) step(0,0,0);
  assert(status.state==SIGN_ROUTE_ARC && status.searching);
  for(i=0;i<10;++i) { observe(1); step(0,0,0); }
  assert(status.state==SIGN_ROUTE_ARC && motor_left==-motor_right && motor_left!=0);
  for(i=0;i<4;++i) step(6,0,0);
  assert(status.state==SIGN_ROUTE_ARC && motor_left>0 && motor_right>0);
  init(100); confirm(1); enter_probe(); leave_probe(0);
  for(i=0;i<260;++i) step(1,0,0);
  assert(status.state==SIGN_ROUTE_CANCELLED && status.fault==2U && !cmd.active);
  for(i=0;i<4;++i) step(6,0,0);
  assert(status.state==SIGN_ROUTE_CANCELLED && motor_left>0 && motor_right>0);
  init(100); confirm(1); enter_probe();
  for(i=0;i<190;++i) { step(15,0,0); assert(status.state!=SIGN_ROUTE_FAULT); }
  init(100); start_arc(1); half_arc(1);
  for(i=0;i<4;++i) step(3,1,1);
  finish_select(1,SIGN_ROUTE_EXIT_CLEAR);
  for(i=0;i<15;++i) step(0,1,1);
  assert(status.state==SIGN_ROUTE_EXIT_CLEAR && !cmd.active);
  for(i=0;i<55;++i) step(0,0,0);
  assert(status.state==SIGN_ROUTE_EXIT_CLEAR && status.searching);
  assert(motor_left==-motor_right && motor_left!=0);
  puts("PASS: navigation bounds never stop tracking/search; live line resumes tracking");
}
static void test_live_line_priority(void)
{
  unsigned i;
  int8_t side;
  for(side=-1;side<=1;side+=2)
  {
    init(100); confirm(side);
    for(i=0;i<50;++i)
    {
      step(6,1,1); /* a recognized sign on a straight cannot take the motors */
      assert(!cmd.active && motor_left>0 && motor_right>0);
    }
    /* Refresh the reservation then observe a bar with no encoder movement. */
    confirm(side); enter_probe();
    for(i=0;i<4;++i) step(6,0,0);
    assert(status.state==SIGN_ROUTE_PROBE && !cmd.active);
    for(i=0;i<9;++i) step(6,0,0);
    assert(status.state==SIGN_ROUTE_ARMED && !cmd.active);
    enter_probe();
    step(0,0,0);
    assert(!cmd.active && motor_left==-motor_right); /* first white sample */
    leave_probe(0);
    assert(status.state==SIGN_ROUTE_SELECTING && !cmd.active);
    step(side<0?8U:1U,0,0);
    assert(cmd.active); /* only steer toward a branch actually under a sensor */
    step(side<0?1U:8U,0,0);
    assert(!cmd.active); /* indicated side has vanished: real line wins */
    step(6,0,0);
    assert(!cmd.active); /* don't keep pivoting while middle sees line */
    step(0,0,0);
    assert(!cmd.active && motor_left==-motor_right);
    for(i=0;i<4;++i) step(6,0,0);
    assert(status.state==SIGN_ROUTE_ARC); /* no minimum yaw gate */
  }
  puts("PASS: sign-only straight, stationary crossbar, probe/selection line priority, immediate white guard");
}
static void test_continuous_search(void)
{
  unsigned phase,i;
  for(phase=0;phase<4;++phase)
  {
    init(UINT32_MAX-1000U);
    if(phase==1) { confirm(-1); enter_probe(); leave_probe(0); }
    if(phase==2) start_arc(-1);
    if(phase==3)
    {
      start_arc(1); half_arc(1);
      for(i=0;i<4;++i) step(3,1,1);
      finish_select(1,SIGN_ROUTE_EXIT_CLEAR);
    }
    for(i=0;i<12000;++i) /* two simulated minutes, no encoder progress */
    {
      if(i%10==0) observe(0);
      step(0,0,0);
      assert(!cmd.active && motor_left==-motor_right && motor_left!=0);
      assert(status.searching && status.state!=SIGN_ROUTE_FAULT);
    }
    step(6,0,0); step(6,0,0);
    assert(!status.searching && motor_left>0 && motor_right>0);
    SimpleLine_Stop(&line); SignRoute_Reset(); step(0,0,0);
    assert(motor_left==0 && motor_right==0); /* operator reset cannot restart */
  }
  puts("PASS: 2-minute search from approach/entry/arc/exit, frame refresh, wrap, reacquisition and STOP");
}
static void test_split_choice(int8_t side)
{
  unsigned i, repeat;
  init(UINT32_MAX-200U); confirm(side); enter_probe();
  /* Brief centre/broad chatter must not repeatedly cancel entry. Digits and
     even contrary arrows cannot overwrite this already committed approach. */
  for(repeat=0;repeat<3;++repeat)
  {
    for(i=0;i<6;++i) { observe(i<3?4:(side<0?1:0)); step(6,0,0); }
    assert(status.state==SIGN_ROUTE_PROBE && status.direction==side);
    step(15,0,0);
  }
  for(i=0;i<5;++i) step(9,0,0);
  assert(status.state==SIGN_ROUTE_SELECTING && status.direction==side && cmd.active);
  assert(side<0 ? (motor_left==0 && motor_right>0) : (motor_right==0 && motor_left>0));
  step(0,0,0); assert(!cmd.active); /* still never drive blind */
  puts("PASS: split arcs choose confirmed side despite digit/arrow noise and centre chatter");
}
static void test_exit_recovery(int8_t side)
{
  unsigned i;
  init(UINT32_MAX-1000U); start_arc(side); half_arc(side);
  for(i=0;i<10;++i) step(15,0,0);
  assert(status.state==SIGN_ROUTE_ARC && !cmd.active); /* photo's all-black */
  for(i=0;i<10;++i) step(9,0,0);
  assert(status.state==SIGN_ROUTE_ARC && !cmd.active); /* ambiguous both sides */
  for(i=0;i<4;++i) step(side<0?12:3,0,0);
  assert(status.state==SIGN_ROUTE_EXIT_SELECT);
  for(i=0;i<260;++i) { observe(side<0?0:1); step(15,0,0); }
  assert(status.state==SIGN_ROUTE_CANCELLED && !cmd.active && status.direction==0);
  assert(motor_left>0 && motor_right>0);
  for(i=0;i<200;++i) { observe(side<0?0:1); step(side<0?12:3,0,0); }
  assert(status.state==SIGN_ROUTE_CANCELLED && !cmd.active); /* same sign cannot retry */
  for(i=0;i<100;++i) { observe(-1); step(6,0,0); }
  assert(status.state==SIGN_ROUTE_IDLE);
  confirm(-side); assert(status.state==SIGN_ROUTE_ARMED && status.direction==-side);
  puts("PASS: wide marks cannot become exits, failed exit withdraws steering and requires fresh rearm");
}
int main(void)
{
  test_exit(-1,100U); test_exit(1,100U);
  test_exit(-1,UINT32_MAX-1000U); test_exit(1,UINT32_MAX-1000U);
  test_crossbar_and_missing_sign(); test_bounds(); test_live_line_priority();
  test_continuous_search();
  test_split_choice(-1); test_split_choice(1);
  test_exit_recovery(-1); test_exit_recovery(1);
  return 0;
}
