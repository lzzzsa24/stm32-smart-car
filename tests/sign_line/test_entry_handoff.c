#include <assert.h>
#include <stdio.h>
#include "simple_line_mode.h"
#include "sign_slowdown.h"

static SimpleLineController line;
static SignRouteStatus status;
static SignRouteCommand command;
static uint32_t now, seq;
static int16_t left, right;
static uint8_t action;
static const int64_t heading = 3700000;

static void step(uint8_t mask, int32_t angle, uint8_t valid, uint8_t paused)
{
  now += 10U;
  SignRoute_UpdateYaw(heading+angle,valid);
  SimpleLine_UpdateYaw(&line,heading+angle,valid,1U);
  SignRoute_Step(mask,now,&command);
  SignRoute_GetStatus(now,&status);
  SimpleLine_StepRoute(&line,mask,&status,&command);
  action=SimpleLine_ResolveRouteOutput(&line,&command,paused,&left,&right);
}

static void observe(int side)
{
  VisionDetection d={0};
  d.class_id=side<0?0:1; d.score=80;
  d.center_x=160; d.center_y=120;
  d.sequence=++seq; d.received_ms=now;
  SignRoute_ObserveDetection(&d);
}

static void init(int side, uint32_t origin)
{
  unsigned i;
  now=origin; seq=0;
  SignRoute_Reset(); SimpleLine_Init(&line); SimpleLine_Start(&line);
  SignRoute_UpdateEncoders(0,0,0,0);
  for(i=0;i<3;++i) { if(side) observe(side); step(6,0,1,0); }
  for(i=0;i<3;++i) step(15,0,1,0);
  assert(status.state==SIGN_ROUTE_PROBE);
}

static void expect_search(int side)
{
  assert(left+right==0 && left!=0);
  assert(side<0 ? left<0 : right<0);
  assert(action==SIMPLE_LINE_SEARCH);
}

static void check_handoff(int side, uint32_t origin)
{
  unsigned i;
  uint8_t opposite=side<0?1U:8U;
  uint8_t selected=side<0?8U:1U;
  int32_t angle=0;
  init(side,origin);
  /* Exact audited sequence: confirmed R, only left outer, then white.
     Final commands must not advance into the wrong branch or circle center. */
  step(opposite,0,1,0); expect_search(side);
  assert(status.direction==side && !command.active);
  step(0,0,1,0); expect_search(side);
  assert(line.line_yaw_mdeg==heading);
  /* Opposite contact cannot refresh the anchor. The return sweep is allowed
     only inside the original selected-side sector, not an expanding U-turn. */
  for(i=0;i<200;++i)
  {
    step(i%2?opposite:0,angle,1,0);
    assert(left+right==0 && left!=0);
    angle += left<0?1000:-1000;
    assert(-side*angle>=0 && -side*angle<=25000);
    assert(line.line_yaw_mdeg==heading);
  }
  step(6,-side*20000,1,0);
  for(i=0;i<5;++i) step(6,-side*20000,1,0);
  assert(!status.entry_line_ready); /* center without chosen outer is not capture */

  init(side,origin);
  step(selected,-side*10000,1,0);
  assert(command.active && left>=0 && right>=0);
  assert(side<0 ? left==0 && right>0 : right==0 && left>0);
  step(0,-side*10000,1,0); expect_search(side);
  step(6,-side*20000,1,0);
  step(6,-side*20000,1,0);
  step(opposite,-side*20000,1,0);
  assert(!status.entry_line_ready); /* center chatter interrupted */
  step(6,-side*20000,1,0);
  now+=60; step(6,-side*20000,1,0);
  assert(!status.entry_line_ready); /* sampling gap cannot complete capture */
  for(i=0;i<3;++i) step(6,-side*20000,1,0);
  assert(status.entry_line_ready && status.state==SIGN_ROUTE_PROBE);
  assert(left>0 && right>0 && !command.active); /* release before fixed 60 degrees */
  step(opposite,-side*20000,1,0);
  assert(left>0 && right>0 && !command.active);
  assert(side<0 ? left>right : right>left); /* real opposite arc curvature now allowed */
  assert(SignSlowdown_ForwardCps(left)>0 && SignSlowdown_ForwardCps(right)>0);
  step(selected,-side*20000,1,0);
  assert(left>0 && right>0 && !command.active); /* route cannot reclaim an acquired line */
  for(i=0;i<4;++i) step(6,-side*80000,1,0);
  assert(status.state==SIGN_ROUTE_ARC);
  step(opposite,-side*70000,1,0);
  assert(left>0 && right>0);

  init(side,origin);
  step(opposite,0,1,1); assert(left==0 && right==0 && action==6U);
  step(selected,0,1,1); assert(left==0 && right==0 && action==6U);
  SimpleLine_Stop(&line); step(selected,0,1,0);
  assert(command.active && left==0 && right==0); /* STOP wins even over active route */
  init(side,origin); step(0,0,0,0);
  assert(status.state==SIGN_ROUTE_CANCELLED && left==0 && right==0);
  init(side,origin); now+=10001; step(opposite,0,1,0);
  assert(status.state==SIGN_ROUTE_CANCELLED && status.direction==0);
  assert(left>0 && right>0); /* cancellation explicitly releases old choice */
}

static void check_late_selection(int side)
{
  unsigned i;
  init(0,100);
  for(i=0;i<4;++i) step(0,0,1,0);
  assert(status.state==SIGN_ROUTE_WAIT_SIGN);
  /* Confirmation arrives inside an already active opposite search sweep. */
  for(i=0;i<3;++i) { observe(side); step(0,-side*10000,1,0); }
  assert(status.state==SIGN_ROUTE_SELECTING);
  expect_search(side);
  step(side<0?1U:8U,0,1,0); expect_search(side);
  step(0,0,1,0); expect_search(side);
}

int main(void)
{
  check_handoff(-1,100); check_handoff(1,100);
  check_handoff(-1,UINT32_MAX-120U); check_handoff(1,UINT32_MAX-120U);
  check_late_selection(-1); check_late_selection(1);
  puts("PASS: actual final outputs, mirrored audited regression, bounded branch search, capture/release, pause/STOP/cancel and wrap");
  return 0;
}
