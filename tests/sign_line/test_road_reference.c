#include <assert.h>
#include <stdio.h>
#include "sign_route.h"
static uint32_t now, seq;
static int32_t counts;
static SignRouteStatus s;
static SignRouteCommand c;
static void step(uint8_t mask, int32_t yaw, int move)
{
  now+=10U; counts+=move;
  SignRoute_UpdateEncoders(counts,counts,counts,counts);
  SignRoute_UpdateYaw(3700000LL+yaw,1);
  SignRoute_Step(mask,now,&c); SignRoute_GetStatus(now,&s);
}
static void reset(uint32_t origin)
{
  SignRoute_SetProfile(SIGN_ROUTE_PROFILE_STANDARD); SignRoute_Reset();
  now=origin; seq=0; counts=0;
  SignRoute_UpdateEncoders(0,0,0,0); step(6,0,0);
}
static void frame(int side)
{
  VisionDetection d={0}; d.class_id=side<0?0:1; d.score=80;
  d.center_x=160; d.center_y=120; d.sequence=++seq; d.received_ms=now;
  SignRoute_ObserveDetection(&d);
}
static void learn(void)
{
  unsigned i;
  for(i=0;i<45;++i) step(6,0,12);
  assert(s.road_reference_valid && s.heading_error_mdeg==0);
}
static void pause_at(int side, int32_t skew)
{
  unsigned i;
  SignRoute_UpdateObservationPause(1,now);
  for(i=0;i<200;++i) { frame(side); step(6,skew,0); }
  SignRoute_UpdateObservationPause(0,now);
  step(6,skew,0);
}
static void arc(int side, int32_t skew)
{
  unsigned i;
  for(i=0;i<3;++i) step(15,skew,8);
  assert(s.state==SIGN_ROUTE_PROBE);
  step(side<0?8:1,skew-side*20000,8);
  for(i=0;i<4;++i) step(6,skew-side*20000,8);
  assert(s.state==SIGN_ROUTE_ARC);
  /* Actual tangent is independent of the skew at the recognition stop. */
  for(i=0;i<30;++i) step(6,-side*(20000+(int32_t)i*1800),16);
  for(i=0;i<50;++i) step(6,side*(-74000+(int32_t)i*2400),16);
  assert(s.state==SIGN_ROUTE_ARC && !c.active);
}
static void skewed_departure(int side, int32_t skew, uint8_t trusted, uint32_t origin)
{
  unsigned i;
  reset(origin); if(trusted) learn();
  pause_at(side,skew);
  assert(s.road_reference_valid==trusted);
  assert(s.heading_error_mdeg==(trusted?skew:0));
  arc(side,skew);
  step(6,side*24000,12); /* natural outward correction, before fixed angle gate */
  if(s.state!=SIGN_ROUTE_EXIT_CLEAR) fprintf(stderr,"departure side=%d skew=%ld trusted=%u state=%u sweep=%ld heading=%ld mm=%ld\n",side,(long)skew,trusted,(unsigned)s.state,(long)s.arc_sweep_mdeg,(long)s.heading_error_mdeg,(long)s.travel_mm);
  assert(s.state==SIGN_ROUTE_EXIT_CLEAR && s.exit_reason==2 && !c.active);
  /* Reacquired straight can be offset from the stopping heading in either direction. */
  for(i=0;i<25;++i) { step(6,0,12); assert(!c.active); }
  assert(s.state==SIGN_ROUTE_LOCKED && !s.direction);
  for(i=0;i<150;++i)
  {
    step(i%10==0?0:(side<0?1:8),side*(int32_t)i*1000,10);
    assert(s.state==SIGN_ROUTE_LOCKED && !s.direction && !c.active);
  }
}
static void negative_evidence(void)
{
  unsigned i;
  reset(100);
  for(i=0;i<300;++i) step(6,0,0); /* stationary, even perfectly centered */
  assert(!s.road_reference_valid);
  for(i=0;i<100;++i) step(15,0,12);
  assert(!s.road_reference_valid);
  for(i=0;i<100;++i) step(6,(int32_t)i*1000,12);
  assert(!s.road_reference_valid); /* continuously curving */
  reset(100);
  /* Production loop runs around 1 ms; valid low-speed wheel edges can arrive
     only every few samples and still describe uninterrupted forward motion. */
  for(i=0;i<1200;++i)
  {
    ++now; if(i%3==0) counts+=2;
    SignRoute_UpdateEncoders(counts,counts,counts,counts);
    SignRoute_UpdateYaw(3700000LL,1);
    SignRoute_Step(6,now,&c); SignRoute_GetStatus(now,&s);
  }
  assert(s.road_reference_valid);
  reset(100); learn(); now+=5001U; step(6,0,0);
  assert(!s.road_reference_valid);
  reset(100); learn(); SignRoute_UpdateYaw(0,0); SignRoute_GetStatus(now,&s);
  assert(!s.road_reference_valid);
  reset(100); pause_at(1,0);
  for(i=0;i<3;++i) step(15,0,8);
  step(1,-20000,8);
  for(i=0;i<4;++i) step(6,-20000,8);
  for(i=0;i<30;++i) step(6,-20000-(int32_t)i*1800,16);
  for(i=0;i<40;++i) step(6,-74000+(int32_t)i*1850,16);
  /* A sizeable correction near the middle of the half-circle is not exit. */
  for(i=0;i<30;++i) step(6,-15000,12);
  assert(s.state==SIGN_ROUTE_ARC && !c.active);
  reset(100); pause_at(1,0); arc(1,0);
  for(i=0;i<100;++i) step(6,45000,0);
  assert(s.state==SIGN_ROUTE_ARC && !c.active); /* stopped, no departure */
  step(15,0,12); step(0,0,12);
  assert(s.state==SIGN_ROUTE_ARC && !c.active); /* ambiguous/white cannot latch */
  step(6,24000,12);
  assert(s.state==SIGN_ROUTE_EXIT_CLEAR && !c.active);
  for(i=0;i<80;++i) { step(i%2?0:8,(int32_t)i*2000,2); assert(!c.active); }
  assert(s.state==SIGN_ROUTE_CANCELLED && s.exit_reason==3 && !s.direction);
  for(i=0;i<100;++i) { step(6,90000,12); assert(!c.active); }
  /* Without a road reference, constant ring curvature must not invent an exit;
     a missed full exit window withdraws instead of waiting for another lap. */
  reset(100); pause_at(1,0); arc(1,0);
  for(i=0;i<100 && s.state==SIGN_ROUTE_ARC;++i)
  { step(6,45000+(int32_t)i*2000,12); assert(!c.active); }
  assert(s.state==SIGN_ROUTE_CANCELLED && s.exit_reason==3);
}
int main(void)
{
  int side,skew,trusted;
  for(side=-1;side<=1;side+=2) for(skew=-30000;skew<=30000;skew+=10000)
    for(trusted=0;trusted<=1;++trusted)
      skewed_departure(side,skew,(uint8_t)trusted,UINT32_MAX-999U);
  negative_evidence();
  puts("PASS: +/-10/20/30-degree stop bias, moving reference, independent natural exit, no delayed turns, missed-window withdrawal and wrap");
  return 0;
}
