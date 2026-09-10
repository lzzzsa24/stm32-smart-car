#include <assert.h>
#include <stdio.h>
#include "promoted_sign_route.h"
static unsigned now, seq;
static int32_t last_input_yaw;
static Promoted_SignRouteStatus s;
static Promoted_SignRouteCommand c;
static void step(unsigned mask, long angle, unsigned valid)
{
  last_input_yaw=(int32_t)angle;
  now+=10; Promoted_SignRoute_UpdateYaw(angle,(uint8_t)valid);
  Promoted_SignRoute_Step((uint8_t)mask,now,&c); Promoted_SignRoute_GetStatus(now,&s);
}
static void finish_outer(int side)
{
  int32_t yaw=last_input_yaw;
  if(s.state==Promoted_SIGN_ROUTE_EXIT_CLEAR || s.state==Promoted_SIGN_ROUTE_EXIT_CLEAR)
  {
    step(6,yaw,1);
    step(side<0?14:7,yaw,1);
    step(6,yaw,1);
  }
}
static void start(int side)
{
  unsigned i; VisionDetection d={0};
  Promoted_SignRoute_Reset(); now=0xFFFFFF00U; seq=0;
  Promoted_SignRoute_UpdateEncoders(0,0,0,0);
  for(i=0;i<45;++i)
  {
    int32_t n=(int32_t)(i+1)*12;
    Promoted_SignRoute_UpdateEncoders(n,n,n,n); step(6,0,1);
  }
  assert(!s.road_reference_valid);
  for(i=0;i<3;++i)
  {
    d.class_id=side<0?0:1; d.score=80; d.center_x=160; d.center_y=120;
    d.sequence=++seq; d.received_ms=now;
    Promoted_SignRoute_ObserveDetection(&d); step(6,0,1);
  }
  for(i=0;i<3;++i) step(15,0,1);
  assert(s.state==Promoted_SIGN_ROUTE_PROBE);
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
  assert(s.state==Promoted_SIGN_ROUTE_ARC); /* verified early line capture enables exit navigation */
  for(i=0;i<4;++i) step(6,-side*80000,1);
  assert(s.state==Promoted_SIGN_ROUTE_ARC);
}
static void test(int side)
{
  unsigned mask;
  for(mask=0;mask<16;++mask) {
    start(side);
    step(mask,side*39999,1); assert(s.state==Promoted_SIGN_ROUTE_ARC && !c.active);
    step(mask,side*40000,1); assert(s.state==Promoted_SIGN_ROUTE_EXIT_CLEAR);
    step(mask,side*25000,1); assert(s.state==Promoted_SIGN_ROUTE_EXIT_CLEAR && c.active && c.heading_drive && s.direction==side);
    step(0,side*25000,1);
    step(mask|(side<0?8U:1U),side*25000,1);
    assert(s.state==Promoted_SIGN_ROUTE_LOCKED && !s.direction && !c.active);
  }
  start(side); step(6,-side*80000,0);
  assert(s.state==Promoted_SIGN_ROUTE_CANCELLED && s.fault==6 && !c.active);
  start(side); step(6,side*121000,1);
  assert(s.state==Promoted_SIGN_ROUTE_EXIT_CLEAR && c.heading_drive && !c.drive_heading_error_mdeg);
  /* Trigger wins even when one sample jumps beyond the old upper bound. */
  now+=6100; step(6,side*121000,1);
  assert(s.state==Promoted_SIGN_ROUTE_EXIT_CLEAR && c.heading_drive);
  step(0,side*121000,0); /* stale IMU cannot switch an active exit to spin search */
  assert(s.state==Promoted_SIGN_ROUTE_EXIT_CLEAR && c.heading_drive && !c.drive_heading_error_mdeg);
  step(0,side*126000,1);
  assert(c.drive_heading_error_mdeg==side*5000);
  finish_outer(side);
  assert(s.state==Promoted_SIGN_ROUTE_LOCKED && !c.heading_drive);

}
static void natural_exit(int side)
{
  unsigned i;
  start(side);
  step(6,side*52000,1); assert(s.state==Promoted_SIGN_ROUTE_EXIT_CLEAR && c.active);
  for(i=0;i<30;++i) step(6,side*35000,1);
  assert(s.state==Promoted_SIGN_ROUTE_EXIT_CLEAR && c.active);
  step(0,0,1); assert(s.state==Promoted_SIGN_ROUTE_EXIT_CLEAR && c.active && c.heading_drive && s.direction==side);
  finish_outer(side); assert(s.state==Promoted_SIGN_ROUTE_LOCKED && !c.active && !s.direction);
  for(i=0;i<20;++i) { step(i%2?0:15,side*90000,1); assert(!c.active); }
  start(side); step(6,side*90000,1); step(6,side*106000,1);
  assert(s.state==Promoted_SIGN_ROUTE_EXIT_CLEAR && c.heading_drive);
  start(side); step(0,side*40000,1); step(0,-side*30000,1);
  assert(s.state==Promoted_SIGN_ROUTE_EXIT_CLEAR && c.active && c.heading_drive && s.direction==side);
  finish_outer(side); assert(s.state==Promoted_SIGN_ROUTE_LOCKED && !s.direction);
  puts("PASS: angle-only start; immediate trigger-heading drive; only outer finishes; stale IMU drops trim, no exit timeout");
}
static void pause_reference_lifetime(void)
{
  unsigned i; VisionDetection d={0};
  Promoted_SignRoute_Reset(); now=UINT32_MAX-999U;
  Promoted_SignRoute_UpdateYaw(10000,1); Promoted_SignRoute_UpdateObservationPause(1,now);
  now+=2000U;
  Promoted_SignRoute_UpdateYaw(20000,1); Promoted_SignRoute_UpdateObservationPause(0,now);
  step(6,35000,1);
  assert(s.approach_from_pause && s.heading_error_mdeg==15000);
  Promoted_SignRoute_UpdateObservationPause(0,now); /* ordinary false samples cannot move the reference */
  step(6,35000,1);
  assert(s.approach_from_pause && s.heading_error_mdeg==15000);
  /* An unconfirmed repeat stop replaces the earlier candidate heading. */
  Promoted_SignRoute_UpdateObservationPause(1,now); now+=2000U;
  Promoted_SignRoute_UpdateYaw(50000,1); Promoted_SignRoute_UpdateObservationPause(0,now);
  step(6,55000,1); assert(s.approach_from_pause && s.heading_error_mdeg==5000);
  /* A completed stop remains primary until route completion/reset or a new stop. */
  now+=5001U;
  for(i=0;i<3;++i)
  {
    d.class_id=0; d.score=80; d.center_x=160; d.center_y=120;
    d.sequence=++seq; d.received_ms=now;
    Promoted_SignRoute_ObserveDetection(&d); step(6,60000,1);
  }
  for(i=0;i<3;++i) step(15,60000,1);
  assert(s.state==Promoted_SIGN_ROUTE_PROBE && s.approach_from_pause && s.heading_error_mdeg==10000);
  Promoted_SignRoute_Reset(); Promoted_SignRoute_UpdateYaw(70000,1); Promoted_SignRoute_UpdateObservationPause(1,now);
  now+=2000U; Promoted_SignRoute_UpdateYaw(80000,0); Promoted_SignRoute_UpdateObservationPause(0,now);
  Promoted_SignRoute_GetStatus(now,&s); assert(!s.approach_from_pause);
  puts("PASS: stopped-heading capture, no continuous overwrite, repeat/expiry/reset and invalid-gyro fallback");
}
static void observation_preserves_driving_deadline(void)
{
  unsigned i;
  Promoted_SignRoute_Reset(); Promoted_SignRoute_SetProfile(Promoted_SIGN_ROUTE_PROFILE_STANDARD);
  now=UINT32_MAX-1900U;
  Promoted_SignRoute_UpdateEncoders(0,0,0,0);
  for(i=0;i<3;++i) step(15,0,1);
  assert(s.state==Promoted_SIGN_ROUTE_PROBE && s.direction==0);
  now+=1500U; step(15,0,1);
  Promoted_SignRoute_UpdateObservationPause(1,now);
  for(i=0;i<200;++i)
  {
    step(15,0,1);
    assert(s.state==Promoted_SIGN_ROUTE_PROBE && !s.fault && !c.active);
  }
  Promoted_SignRoute_UpdateObservationPause(0,now);
  for(i=0;i<25;++i) step(15,0,1);
  assert(s.state==Promoted_SIGN_ROUTE_PROBE && !s.fault);
  /* Preserve remaining driving time, rather than spending it while stopped
     or restarting an entire new timeout window at resumption. */
  now+=100U; step(15,0,1);
  assert(s.state==Promoted_SIGN_ROUTE_IDLE && s.fault==1 && !c.active);
  puts("PASS: observation pause excludes stopped time from navigation deadline; remaining bounded timeout and clock wrap retained");
}
int main(void)
{
  observation_preserves_driving_deadline();
  test(-1); test(1);
  natural_exit(-1); natural_exit(1);
  pause_reference_lifetime();
  puts("PASS: MPU yaw gates mirrored entry/half-circle/exit; stale and wrong-way withdraw; tick wrap");
  return 0;
}
