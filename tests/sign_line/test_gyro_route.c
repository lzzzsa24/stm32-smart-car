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
  step(6,0,1); assert(!c.active); /* crossbar on straight approach is not a fork */
  step(side<0?1:8,0,1); /* opposite-only cannot authorize driving across white */
  assert(!c.active && s.direction==side);
  step(0,-side*10000,1);
  assert(!c.active && s.direction==side);
  step(side<0?8:1,-side*20000,1);
  assert(c.active);
  step(0,-side*20000,1); assert(!c.active);
  for(i=0;i<4;++i)
  { step(6,-side*20000,1); assert(!c.active && s.direction==side); }
  assert(s.state==SIGN_ROUTE_ARC); /* verified early line capture enables exit navigation */
  for(i=0;i<4;++i) step(6,-side*80000,1);
  assert(s.state==SIGN_ROUTE_ARC);
}
static void test(int side)
{
  unsigned i,j; long entry=-side*80000;
  static const uint8_t ambiguous[]={0,5,7,9,10,11,13,14,15};
  start(side);
  step(6,entry+side*20000,1);
  step(6,entry-side*10000,1);
  assert(s.state==SIGN_ROUTE_ARC && s.yaw_mdeg==0); /* stationary yaw must not lock apex */
  start(side);
  SignRoute_UpdateEncoders(2000,2000,2000,2000);
  for(i=0;i<4;++i) step(side<0?12:3,entry+side*70000,1);
  assert(s.state==SIGN_ROUTE_ARC); /* early edge cannot exit */
  step(6,entry-side*20000,1);
  assert(s.state==SIGN_ROUTE_ARC && side*s.yaw_mdeg==-20000); /* locked apex cannot drift */
  step(6,entry+side*70000,1);
  assert(side*s.yaw_mdeg==70000);
  for(j=0;j<sizeof(ambiguous);++j)
  {
    for(i=0;i<4;++i) step(ambiguous[j],entry+side*170000,1);
    assert(s.state==SIGN_ROUTE_ARC && !c.active); /* no exit from white, wide or both sides */
  }
  for(i=0;i<4;++i) step(side<0?12:3,entry+side*170000,1);
  assert(s.state==SIGN_ROUTE_EXIT_SELECT && !c.active);
  for(i=0;i<4;++i) step(6,entry+side*170000,1);
  assert(s.state==SIGN_ROUTE_EXIT_SELECT); /* no yaw: not completed */
  for(i=0;i<4;++i) step(0,0,1);
  assert(s.state==SIGN_ROUTE_EXIT_CLEAR);
  assert(c.active && c.left_pwm==c.right_pwm && c.left_pwm>0);
  SignRoute_UpdateEncoders(2500,2500,2500,2500);
  for(i=0;i<4;++i) step(6,0,1);
  assert(s.state==SIGN_ROUTE_LOCKED);
  start(side);
  SignRoute_UpdateEncoders(2000,2000,2000,2000);
  for(i=0;i<4;++i) step(6,entry+side*170000,1);
  for(i=0;i<4;++i) step(0,entry+side*170000,1);
  for(i=0;i<4;++i) step(0,0,1);
  assert(s.state==SIGN_ROUTE_EXIT_CLEAR);
  now+=2100; step(0,0,1);
  assert(s.state==SIGN_ROUTE_CANCELLED && !c.active);
  start(side);
  SignRoute_UpdateEncoders(2000,2000,2000,2000);
  for(i=0;i<4;++i) step(6,entry+side*170000,1);
  for(i=0;i<4;++i) step(0,entry+side*170000,1);
  step(0,-side*20000,1);
  assert(s.state==SIGN_ROUTE_EXIT_SELECT); /* 20-degree miss must not drive straight */
  assert(side<0 ? c.left_pwm>0 && c.right_pwm==0 : c.right_pwm>0 && c.left_pwm==0);
  step(0,-side*5000,1);
  assert(s.state==SIGN_ROUTE_EXIT_CLEAR && c.left_pwm==c.right_pwm);
  start(side);
  SignRoute_UpdateEncoders(2000,2000,2000,2000);
  for(i=0;i<4;++i) step(6,entry+side*170000,1);
  for(i=0;i<4;++i) step(0,entry+side*170000,1);
  now+=6100; step(0,entry+side*170000,1);
  assert(s.state==SIGN_ROUTE_CANCELLED && !c.active); /* stalled alignment remains bounded */
  start(side); step(6,entry,0);
  assert(s.state==SIGN_ROUTE_CANCELLED && s.fault==6 && !c.active);
  start(side); step(6,entry-side*100000,1);
  assert(s.state==SIGN_ROUTE_CANCELLED && !c.active); /* wrong half */
}
static void natural_exit(int side)
{
  unsigned i;
  long entry=-side*80000;
  start(side);
  SignRoute_UpdateEncoders(2000,2000,2000,2000);
  for(i=0;i<4;++i) step(6,entry+side*160000,1);
  assert(s.state==SIGN_ROUTE_ARC && s.arc_peak_mdeg==160000);
  for(i=0;i<4;++i) step(6,0,1);
  assert(s.state==SIGN_ROUTE_LOCKED && !c.active && s.direction==0 && s.heading_error_mdeg==0);
  step(side<0?1:8,0,1); assert(!c.active); /* no forced straight over an outer contact */
  SignRoute_UpdateEncoders(2500,2500,2500,2500);
  for(i=0;i<4;++i) step(6,0,1);
  assert(s.state==SIGN_ROUTE_LOCKED && !c.active && s.direction==0);
  for(i=0;i<5;++i) step(0,entry+side*175000,1);
  assert(s.state==SIGN_ROUTE_LOCKED && !c.active); /* no late exit-turn after natural rejoin */

  start(side);
  SignRoute_UpdateEncoders(2000,2000,2000,2000);
  for(i=0;i<4;++i) step(6,entry+side*170000,1);
  step(0,entry+side*186000,1);
  assert(s.state==SIGN_ROUTE_CANCELLED && !c.active); /* error worsens >15 deg: withdraw */

  start(side);
  step(6,-side*120000,1); /* permitted entry apex at the configured boundary */
  SignRoute_UpdateEncoders(2000,2000,2000,2000);
  for(i=0;i<4;++i) step(6,side*50000,1);
  assert(s.state==SIGN_ROUTE_EXIT_SELECT);
  step(0,side*10000,1); /* aligned after 40 degrees: old extra 45-degree gate blocked this */
  assert(s.state==SIGN_ROUTE_EXIT_CLEAR && c.active && c.left_pwm==c.right_pwm);
  puts("PASS: natural exit below 170deg, aligned heading requires no extra 45deg, live rejoin and divergent-heading withdrawal");
}
int main(void)
{
  test(-1); test(1);
  natural_exit(-1); natural_exit(1);
  puts("PASS: MPU yaw gates mirrored entry/half-circle/exit; stale and wrong-way withdraw; tick wrap");
  return 0;
}
