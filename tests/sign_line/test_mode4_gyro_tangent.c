#include <assert.h>
#include <stdio.h>
#include "sign_route.h"
#include "sign_route_config.h"

static uint32_t now, sequence;
static int32_t counts[4];
static SignRouteStatus status;
static SignRouteCommand command;

static void step(uint8_t mask, int64_t yaw_mdeg, uint8_t yaw_valid, int32_t count_delta)
{
  unsigned i;
  now += 10U;
  for (i=0;i<4;++i) counts[i] += count_delta;
  SignRoute_UpdateEncoders(counts[0],counts[1],counts[2],counts[3]);
  SignRoute_UpdateYaw(yaw_mdeg,yaw_valid);
  SignRoute_Step(mask,now,&command);
  SignRoute_GetStatus(now,&status);
}

static void observe(int side)
{
  VisionDetection detection={0};
  detection.class_id=side<0?0:1; detection.score=80;
  detection.center_x=160; detection.center_y=120;
  detection.sequence=++sequence; detection.received_ms=now;
  SignRoute_ObserveDetection(&detection);
}

static void begin_entry(int side, SignRouteProfile profile)
{
  unsigned i;
  now=100U; sequence=0U;
  for(i=0;i<4;++i) counts[i]=0;
  SignRoute_Reset(); SignRoute_SetProfile(profile);
  SignRoute_UpdateEncoders(0,0,0,0);
  for(i=0;i<3;++i) { observe(side); step(6,0,1,0); }
  assert(status.state==SIGN_ROUTE_ARMED && status.direction==side);
  for(i=0;i<3;++i) step(15,0,1,0);
  assert(status.state==SIGN_ROUTE_PROBE);
}

static void reach_arc(int side, SignRouteProfile profile)
{
  unsigned i;
  uint8_t selected=side<0?8U:1U;
  begin_entry(side,profile);
  step(selected,-side*30000LL,1,0);
  assert(command.active);
  if(profile==SIGN_ROUTE_PROFILE_GYRO_TANGENT)
  {
    assert(command.gentle_arc);
    assert(command.left_pwm==(side<0?SIGN_GYRO_TANGENT_INNER_PWM:SIGN_GYRO_TANGENT_OUTER_PWM));
    assert(command.right_pwm==(side<0?SIGN_GYRO_TANGENT_OUTER_PWM:SIGN_GYRO_TANGENT_INNER_PWM));
    assert(command.left_pwm>0 && command.right_pwm>0);
  }
  else assert(!command.gentle_arc && (!command.left_pwm || !command.right_pwm));
  for(i=0;i<4;++i) step(6,-side*SIGN_GYRO_TANGENT_ENTRY_MDEG,1,0);
  assert(status.state==SIGN_ROUTE_ARC);
}

static void mode4_trajectory(int side)
{
  unsigned i;
  int32_t angle;
  int64_t entry_yaw=-side*SIGN_GYRO_TANGENT_ENTRY_MDEG;
  reach_arc(side,SIGN_ROUTE_PROFILE_GYRO_TANGENT);
  assert(status.profile==SIGN_ROUTE_PROFILE_GYRO_TANGENT);

  /* Sensor masks cannot send the car around another lap: gyro owns this phase. */
  for(angle=10000;angle<SIGN_GYRO_TANGENT_ARC_MDEG;angle+=10000)
  {
    uint8_t mask=(angle/10000)%3==0?15U:((angle/10000)%3==1?0U:(side<0?1U:8U));
    step(mask,entry_yaw+side*angle,1,angle==20000?1200:0);
    assert(status.state==SIGN_ROUTE_ARC && command.active && command.gentle_arc);
    assert(command.left_pwm>0 && command.right_pwm>0);
    assert(command.left_pwm==(side<0?SIGN_GYRO_TANGENT_OUTER_PWM:SIGN_GYRO_TANGENT_INNER_PWM));
    assert(command.right_pwm==(side<0?SIGN_GYRO_TANGENT_INNER_PWM:SIGN_GYRO_TANGENT_OUTER_PWM));
  }
  for(i=0;i<4;++i)
    step(15,entry_yaw+side*SIGN_GYRO_TANGENT_ARC_MDEG,1,0);
  if(status.state!=SIGN_ROUTE_EXIT_SELECT || !command.active || !command.gentle_arc)
    fprintf(stderr,"mode4 trigger side=%d state=%d fault=%u mm=%ld yaw=%ld cmd=%u gentle=%u %d/%d\n",
        side,(int)status.state,status.fault,(long)status.travel_mm,(long)status.yaw_mdeg,
        command.active,command.gentle_arc,command.left_pwm,command.right_pwm);
  assert(status.state==SIGN_ROUTE_EXIT_SELECT && command.active && command.gentle_arc);
  assert(command.left_pwm>0 && command.right_pwm>0);
  assert(command.left_pwm==(side<0?SIGN_GYRO_TANGENT_INNER_PWM:SIGN_GYRO_TANGENT_OUTER_PWM));

  /* The diagonal exit uses the original entry turn direction until the
     approach heading is recovered, then drives straight to the outgoing line. */
  step(0,side*30000LL,1,0);
  assert(status.state==SIGN_ROUTE_EXIT_SELECT && command.gentle_arc);
  step(0,0,1,0);
  assert(status.state==SIGN_ROUTE_EXIT_CLEAR && command.active);
  assert(!command.gentle_arc && command.left_pwm==command.right_pwm);
  step(15,0,1,600);
  assert(status.state==SIGN_ROUTE_EXIT_CLEAR && command.active);
  for(i=0;i<4;++i)
  {
    step(6,0,1,0);
    if(i<3U)
    {
      assert(status.state==SIGN_ROUTE_EXIT_CLEAR && command.active);
      assert(command.left_pwm==command.right_pwm);
    }
  }
  assert(status.state==SIGN_ROUTE_LOCKED && !command.active);
}

static void profile_isolation_and_invalid_gyro(void)
{
  reach_arc(-1,SIGN_ROUTE_PROFILE_STANDARD);
  step(6,60000-30000,1,1000);
  assert(status.state==SIGN_ROUTE_ARC && !command.active);
  begin_entry(1,SIGN_ROUTE_PROFILE_GYRO_TANGENT);
  step(1,-30000,0,0);
  assert(status.state==SIGN_ROUTE_CANCELLED && !command.active && status.fault==6U);
  puts("PASS: standard profile unchanged; invalid mode-4 gyro withdraws route ownership without a stop command");
}

int main(void)
{
  SignRoute_Init();
  mode4_trajectory(-1); mode4_trajectory(1);
  puts("PASS: mirrored mode-4 gyro entry, 165-degree half-arc, heading recovery and straight line capture");
  profile_isolation_and_invalid_gyro();
  return 0;
}
