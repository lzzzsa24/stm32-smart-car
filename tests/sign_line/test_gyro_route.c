#include <assert.h>
#include <stdio.h>
#include "sign_route.h"
static unsigned now, seq;
static SignRouteStatus s;
static SignRouteCommand c;
static void step(unsigned mask, long angle, unsigned valid)
{
  now+=10; SignRoute_UpdateYaw(angle,(uint8_t)valid);
  SignRoute_Step((uint8_t)mask,now,&c); SignRoute_GetStatus(now,&s);
}
static void start(int side)
{
  unsigned i; VisionDetection d={0};
  SignRoute_Reset(); now=0xFFFFFF00U; seq=0;
  SignRoute_UpdateEncoders(0,0,0,0);
  for(i=0;i<3;++i)
  {
    d.class_id=side<0?0:1; d.score=80; d.center_x=160; d.center_y=120;
    d.sequence=++seq; d.received_ms=now;
    SignRoute_ObserveDetection(&d); step(6,0,1);
  }
  for(i=0;i<3;++i) step(15,0,1);
  assert(s.state==SIGN_ROUTE_PROBE);
  step(side<0?8:1,-side*20000,1);
  for(i=0;i<4;++i) step(6,-side*20000,1);
  assert(s.state==SIGN_ROUTE_PROBE); /* line capture alone cannot finish */
  for(i=0;i<4;++i) step(6,-side*80000,1);
  assert(s.state==SIGN_ROUTE_ARC);
}
static void test(int side)
{
  unsigned i; long entry=-side*80000;
  start(side);
  SignRoute_UpdateEncoders(2000,2000,2000,2000);
  for(i=0;i<4;++i) step(side<0?12:3,entry+side*70000,1);
  assert(s.state==SIGN_ROUTE_ARC); /* early edge cannot exit */
  for(i=0;i<4;++i) step(15,entry+side*170000,1);
  assert(s.state==SIGN_ROUTE_ARC); /* wide bar still not exit */
  for(i=0;i<4;++i) step(side<0?12:3,entry+side*170000,1);
  assert(s.state==SIGN_ROUTE_EXIT_SELECT);
  for(i=0;i<4;++i) step(6,entry+side*170000,1);
  assert(s.state==SIGN_ROUTE_EXIT_SELECT); /* no yaw: not completed */
  for(i=0;i<4;++i) step(6,0,1);
  assert(s.state==SIGN_ROUTE_EXIT_CLEAR);
  SignRoute_UpdateEncoders(2500,2500,2500,2500);
  for(i=0;i<4;++i) step(6,0,1);
  assert(s.state==SIGN_ROUTE_LOCKED);
  start(side); step(6,entry,0);
  assert(s.state==SIGN_ROUTE_CANCELLED && s.fault==6 && !c.active);
  start(side); step(6,entry-side*100000,1);
  assert(s.state==SIGN_ROUTE_CANCELLED && !c.active); /* wrong half */
}
int main(void)
{
  test(-1); test(1);
  puts("PASS: MPU yaw gates mirrored entry/half-circle/exit; stale and wrong-way withdraw; tick wrap");
  return 0;
}
