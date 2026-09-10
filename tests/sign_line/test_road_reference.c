#include <assert.h>
#include <stdio.h>
#include "sign_route.h"
static uint32_t now,seq;
static int32_t counts;
static int32_t last_input_yaw;
static SignRouteStatus s;
static SignRouteCommand c;
static void step(uint8_t mask,int32_t yaw,int move)
{
  last_input_yaw=yaw;
  now+=10U; counts+=move;
  SignRoute_UpdateEncoders(counts,counts,counts,counts);
  SignRoute_UpdateYaw(3700000LL+yaw,1);
  SignRoute_Step(mask,now,&c); SignRoute_GetStatus(now,&s);
}
static void finish_outer(int side)
{
  int32_t yaw=last_input_yaw;
  if(s.state==SIGN_ROUTE_EXIT_SELECT || s.state==SIGN_ROUTE_EXIT_CLEAR)
  {
    step(6,yaw,12);
    step(side<0?14:7,yaw,12);
    step(6,yaw,12);
  }
}
static void frame(int side)
{
  VisionDetection d={0}; d.class_id=side<0?0:1; d.score=80;
  d.center_x=160; d.center_y=120; d.sequence=++seq; d.received_ms=now;
  SignRoute_ObserveDetection(&d);
}
static void start(int side,int32_t stopped)
{
  unsigned i;
  SignRoute_SetProfile(SIGN_ROUTE_PROFILE_STANDARD); SignRoute_Reset();
  now=UINT32_MAX-999U; seq=0; counts=0;
  SignRoute_UpdateEncoders(0,0,0,0); step(6,stopped-35000,0);
  for(i=0;i<50;++i) step(6,stopped-35000,12);
  assert(!s.road_reference_valid); /* moving samples cannot choose the reference */
  SignRoute_UpdateObservationPause(1,now);
  for(i=0;i<200;++i) { frame(side); step(6,stopped,0); assert(!c.active); }
  SignRoute_UpdateObservationPause(0,now);
  step(6,stopped,0);
  assert(s.road_reference_valid && s.approach_from_pause && s.heading_error_mdeg==0);
  for(i=0;i<10;++i) step(6,stopped+10000,12);
  assert(s.heading_error_mdeg==10000); /* later tracking cannot overwrite stop */
  for(i=0;i<3;++i) step(15,stopped,12);
  step(side<0?8:1,stopped-side*20000,12);
  for(i=0;i<4;++i) step(6,stopped-side*20000,12);
  assert(s.state==SIGN_ROUTE_ARC);
}
static void stopped_exit(int side,int32_t stopped,uint8_t edge,int32_t apex)
{
  unsigned i; int32_t threshold=edge?30000:40000;
  uint8_t mask=edge?(side<0?8:1):6;
  start(side,stopped);
  step(6,stopped-side*apex,1500);
  for(i=0;i<4;++i) step(mask,stopped-side*threshold,12);
  assert(s.state==SIGN_ROUTE_ARC && !c.active);
  for(i=0;i<4;++i) step(6,stopped,12);
  assert(s.state==SIGN_ROUTE_ARC && !c.active); /* midpoint zero is not exit */
  for(i=0;i<4;++i) step(mask,stopped+side*(threshold-1000),12);
  assert(s.state==SIGN_ROUTE_ARC && !c.active);
  for(i=0;i<4;++i) step(mask,stopped+side*threshold,12);
  assert(s.state==SIGN_ROUTE_EXIT_SELECT && c.active);
  assert(s.heading_error_mdeg==side*threshold && s.approach_from_pause);
  step(6,stopped+side*26000,12);
  assert(s.state==SIGN_ROUTE_EXIT_SELECT && c.active);
  step(6,stopped+side*25000,12);
  assert(s.state==SIGN_ROUTE_EXIT_CLEAR && !c.active); /* widened alignment */
  for(i=0;i<4;++i) step(6,stopped+side*25000,12);
  finish_outer(side);  assert(s.state==SIGN_ROUTE_LOCKED && !s.direction);
  for(i=0;i<100;++i) { step(i%2?0:8,stopped+side*90000,12); assert(!c.active); }
}
static void natural_exit(int side,int32_t stopped)
{
  unsigned i;
  start(side,stopped); step(6,stopped-side*80000,1500);
  step(6,stopped+side*52000,12);
  step(6,stopped+side*35000,12);
  assert(s.state==SIGN_ROUTE_ARC && !c.active); /* one return cannot skip the turn */
  for(i=0;i<30;++i) { step(6,stopped+side*35000,12); assert(!c.active); }
  assert(s.state==SIGN_ROUTE_EXIT_CLEAR && s.exit_reason==2);
  finish_outer(side);  assert(s.state==SIGN_ROUTE_LOCKED && !s.direction);
  for(i=0;i<100;++i) { step(i%2?0:8,stopped+side*90000,12); assert(!c.active); }
  start(side,stopped); step(6,stopped-side*80000,1500);
  step(6,stopped+side*52000,12); step(6,stopped+side*35000,12);
  for(i=0;i<30;++i) step(6,stopped+side*35000,12);
  assert(s.state==SIGN_ROUTE_EXIT_CLEAR);
  for(i=0;i<80;++i) { step(i%2?0:(side<0?7:14),stopped+side*35000,2); assert(!c.active); }
  assert(s.state==SIGN_ROUTE_CANCELLED && !s.direction); /* no late turn */
}
int main(void)
{
  int side,offset,edge,apex; unsigned i;
  for(side=-1;side<=1;side+=2) for(offset=-30000;offset<=30000;offset+=10000)
  {
    for(edge=0;edge<=1;++edge) for(apex=20000;apex<=100000;apex+=80000)
      stopped_exit(side,offset,(uint8_t)edge,apex);
    natural_exit(side,offset);
  }
  start(1,0); step(6,-110000,1500); step(6,110000,12);
  assert(s.arc_sweep_mdeg>200000 && s.state==SIGN_ROUTE_ARC && !s.fault);
  /* No repeated valid line samples: a sweep above 200 is diagnostic only. */
  start(1,0); step(6,-80000,1500);
  for(i=0;i<8;++i) step(i%2?15:9,55000,12);
  assert(s.state==SIGN_ROUTE_ARC && !c.active);
  for(i=0;i<4;++i) step(6,55000,12);
  step(0,45000,0); step(8,40000,12);
  assert(s.state==SIGN_ROUTE_EXIT_CLEAR && !c.active); /* reacquisition before alignment */
  puts("PASS: stopped pose is primary, moving/entry yaw cannot replace it; mirrored 30/40 exit, 25 alignment, 35 natural return and no sweep gates");
  return 0;
}
