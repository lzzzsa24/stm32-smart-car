#include <assert.h>
#include <stdio.h>
#include "sign_route.h"
#include "sign_route_config.h"
#include "simple_line_mode.h"
static SimpleLineController line;
static SignRouteStatus status;
static SignRouteCommand cmd;
static unsigned seq;
static void frame(int cls, unsigned now)
{
  VisionDetection d={0};
  d.class_id=(int8_t)cls; d.score=70; d.center_x=160; d.center_y=120;
  d.sequence=++seq; d.received_ms=now; SignRoute_ObserveDetection(&d);
}
static void step(unsigned now, unsigned mask)
{
  SignRoute_Step((uint8_t)mask,now,&cmd);
  SignRoute_GetStatus(now,&status);
  if(status.state==SIGN_ROUTE_PROBE && status.direction)
    SimpleLine_SetDirection(&line,status.direction);
  SimpleLine_Step(&line,(uint8_t)mask);
}
static void test(int side, unsigned origin)
{
  unsigned i, start;
  SignRoute_Reset(); SimpleLine_Init(&line); SimpleLine_Start(&line); seq=0;
  for(i=0;i<3;++i) { frame(side<0?0:1,origin+i*10); step(origin+i*10,6); }
  for(i=0;i<3;++i) step(origin+30+i*10,15);
  start=origin+50;
  assert(status.state==SIGN_ROUTE_PROBE && status.direction==side);
  for(i=0;i<SIGN_PROBE_HOLD_MS;++i)
  {
    unsigned mask=i%16;
    if(i%100==0) frame(i%300==0?4:(side<0?1:0),start+i);
    /* No odometry, then absurd odometry: neither cancels the requested hold. */
    if(i==5000) SignRoute_UpdateEncoders(200000,200000,-200000,-200000);
    step(start+i,mask);
    assert(status.state==SIGN_ROUTE_PROBE && status.direction==side);
    if(mask & (side<0?8:1))
    {
      assert(cmd.active);
      assert(side<0 ? cmd.left_pwm==0 && cmd.right_pwm>0 : cmd.right_pwm==0 && cmd.left_pwm>0);
    }
    else assert(!cmd.active);
    if(!mask) assert(side<0 ? line.left_pwm<0 && line.right_pwm>0 : line.right_pwm<0 && line.left_pwm>0);
  }
  step(start+10000,6);
  assert(status.state==SIGN_ROUTE_SELECTING && status.direction==side);
  for(i=1;i<=4;++i) step(start+10000+i*10,6);
  assert(status.state==SIGN_ROUTE_ARC); /* elapsed hold doesn't cancel next phase */
  SignRoute_Reset(); SimpleLine_Stop(&line); step(start+10100,0);
  assert(!cmd.active && line.left_pwm==0 && line.right_pwm==0);
}
int main(void)
{
  test(-1,100); test(1,100); test(-1,0xFFFFF000U); test(1,0xFFFFF000U);
  puts("PASS: real 10-second hold, all masks, mirrored search, noise, wrap, expiry and STOP");
  return 0;
}
