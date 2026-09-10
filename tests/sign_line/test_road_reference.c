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
static void stopped_exit(int side,int32_t stopped,uint8_t mask,int32_t apex)
{
  unsigned i; uint8_t outer=side<0?8:1;
  start(side,stopped);
  step(6,stopped-side*apex,0);
  step(mask,stopped,0);
  assert(s.state==SIGN_ROUTE_ARC && !c.active);
  step(mask,stopped+side*39999,0);
  assert(s.state==SIGN_ROUTE_ARC && !c.active);
  now+=80;
  step(mask,stopped+side*40000,0);
  assert(s.state==SIGN_ROUTE_EXIT_SELECT && s.travel_mm==0);
  assert(s.heading_error_mdeg==side*40000 && s.approach_from_pause);
  step(mask,stopped+side*25000,0);
  assert(s.state==SIGN_ROUTE_EXIT_CLEAR && !c.active && s.direction==side);
  step(0,stopped+side*25000,0);
  step((uint8_t)(mask|outer),stopped+side*25000,0);
  assert(s.state==SIGN_ROUTE_LOCKED && !s.direction && !c.active);
  for(i=0;i<100;++i) { step(i%2?0:8,stopped+side*90000,12); assert(!c.active); }
}
static void natural_exit(int side,int32_t stopped)
{
  unsigned i;
  start(side,stopped);
  step(6,stopped+side*52000,0);
  assert(s.state==SIGN_ROUTE_EXIT_SELECT && c.active);
  for(i=0;i<30;++i) step(6,stopped+side*35000,12);
  assert(s.state==SIGN_ROUTE_EXIT_SELECT && c.active); /* no natural-return shortcut */
  step(6,stopped,0);
  assert(s.state==SIGN_ROUTE_EXIT_CLEAR && s.direction==side && !c.active);
  finish_outer(side); assert(s.state==SIGN_ROUTE_LOCKED && !s.direction);
}
int main(void)
{
  int side,offset,apex; unsigned mask;
  for(side=-1;side<=1;side+=2) for(offset=-30000;offset<=30000;offset+=10000)
  {
    for(mask=0;mask<16;++mask) for(apex=20000;apex<=100000;apex+=80000)
      stopped_exit(side,offset,(uint8_t)mask,apex);
    natural_exit(side,offset);
  }
  puts("PASS: stopped reference and biased poses; all16 masks start at40 with zero travel and sample gap; completion still requires outer clear/black");
  return 0;
}
