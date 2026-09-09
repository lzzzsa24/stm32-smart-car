#include <assert.h>
#include <stdio.h>
#include "simple_line_mode.h"

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
  for(i=0;i<400;++i)
  {
    step(i%2?opposite:0,angle,1,0);
    assert(left+right==0 && left!=0);
    angle += left<0?1000:-1000;
    assert(-side*angle>=0 && -side*angle<=SIMPLE_LINE_ENTRY_SEARCH_SECTOR_MDEG);
    assert(line.line_yaw_mdeg==heading);
  }
  step(6,-side*20000,1,0);
  for(i=0;i<5;++i) step(6,-side*20000,1,0);
  assert(!status.entry_line_ready); /* center without chosen outer is not capture */

  init(side,origin);
  step(selected,-side*90000,1,0); /* a late contact must not shift the far bound */
  angle=-side*90000;
  for(i=0;i<160;++i)
  {
    step(0,angle,1,0);
    angle += left<0?1000:-1000;
    assert(-side*angle>=0 && -side*angle<=SIMPLE_LINE_ENTRY_SEARCH_SECTOR_MDEG);
    assert(status.state==SIGN_ROUTE_PROBE);
  }

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
  assert(status.entry_line_ready && status.state==SIGN_ROUTE_ARC);
  assert(left>0 && right>0 && !command.active); /* release before fixed 60 degrees */
  step(opposite,-side*20000,1,0);
  assert(left>0 && right>0 && !command.active);
  assert(side<0 ? left>right : right>left); /* real opposite arc curvature now allowed */
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

static void check_early_arc(int side)
{
  unsigned i;
  int angle;
  int32_t counts=0;
  init(side,100);
  step(side<0?8U:1U,-side*20000,1,0);
  for(i=0;i<4;++i) step(6,-side*20000,1,0);
  /* The line handoff is already verified; leaving navigation in PROBE
     makes a smooth early capture miss every subsequent exit decision. */
  assert(status.state==SIGN_ROUTE_ARC);
  /* 20-degree line capture, entry keeps turning to its 80-degree apex.
     The opposite semicircle must be measured from that apex, not capture. */
  step(0,-side*20000,1,0);
  expect_search(side); /* entering ARC alone must not invent opposite curvature */
  for(angle=21;angle<=80;++angle)
  {
    counts+=22; SignRoute_UpdateEncoders(counts,counts,counts,counts);
    now+=40; step(side<0?8U:1U,-side*angle*1000,1,0);
    assert(status.state==SIGN_ROUTE_ARC && status.yaw_mdeg==0);
    assert(left>0 && right>0 && !command.active);
  }
  for(angle=1;angle<=171;++angle)
  {
    counts+=22; SignRoute_UpdateEncoders(counts,counts,counts,counts);
    now+=40; step(6,side*(angle-80)*1000,1,0);
    if(angle<171) assert(status.state==SIGN_ROUTE_ARC);
  }
  assert(status.state==SIGN_ROUTE_EXIT_SELECT);
  /* Low-speed alignment takes four seconds; the former 2.5s timer cancelled
     it before a target heading could be reached. */
  for(angle=90;angle>=12;--angle)
  {
    now+=40; step(0,side*angle*1000,1,0);
    assert(status.state==SIGN_ROUTE_EXIT_SELECT);
    assert(command.active && (side<0 ? left==0 && right>0 : right==0 && left>0));
  }
  now+=90; step(0,-side*12000,1,0); /* crosses over the entire +/-10-degree band */
  assert(status.state==SIGN_ROUTE_EXIT_CLEAR && left==right && left>0);
  for(i=0;i<20;++i)
  {
    counts+=22; SignRoute_UpdateEncoders(counts,counts,counts,counts);
    now+=40; step(0,-side*12000,1,0);
    assert(status.state==SIGN_ROUTE_EXIT_CLEAR && left==right && left>0);
  }
  for(i=0;i<2;++i) { now+=40; step(6,-side*12000,1,0); }
  assert(status.state==SIGN_ROUTE_LOCKED);
}

int main(void)
{
  check_early_arc(-1); check_early_arc(1);
  check_handoff(-1,100); check_handoff(1,100);
  check_handoff(-1,UINT32_MAX-120U); check_handoff(1,UINT32_MAX-120U);
  check_late_selection(-1); check_late_selection(1);
  puts("PASS: actual final outputs, mirrored audited regression, bounded branch search, capture/release, pause/STOP/cancel and wrap");
  return 0;
}
