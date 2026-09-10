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

static void begin_entry_turn(int side)
{
  unsigned i;
  now=100U; sequence=0U;
  for(i=0;i<4;++i) counts[i]=0;
  SignRoute_Reset(); SignRoute_SetProfile(SIGN_ROUTE_PROFILE_GYRO_TANGENT);
  SignRoute_UpdateEncoders(0,0,0,0);
  SignRoute_UpdateYaw(0,1);
  SignRoute_UpdateObservationPause(1U,now);
  for(i=0;i<3;++i) { observe(side); step(6,0,1,0); }
  assert(status.state==SIGN_ROUTE_ARMED && status.direction==side);

  /* No crossbar or outer sensor is needed after the completed recognition
     stop: its falling edge starts the fixed-angle turn at the marked point. */
  SignRoute_UpdateObservationPause(0U,now);
  step(6,0,1,0);
  assert(status.state==SIGN_ROUTE_PROBE && command.active && !command.gentle_arc);
  assert(side<0 ? (command.left_pwm<0 && command.right_pwm>0) :
                  (command.right_pwm<0 && command.left_pwm>0));
  assert(command.left_pwm==-command.right_pwm);
}

static void reach_arc(int side)
{
  unsigned i;
  int64_t entry_yaw=-side*SIGN_GYRO_TANGENT_ENTRY_MDEG;
  begin_entry_turn(side);

  step(6,entry_yaw,1,0);
  assert(status.state==SIGN_ROUTE_SELECTING && command.active);
  assert(command.left_pwm==command.right_pwm && command.left_pwm>0);

  /* Leave the original centre line, drive the blue diagonal, then capture the
     first narrow line of the selected circle. */
  step(0,entry_yaw,1,400);
  for(i=0;i<4;++i) step(side<0?8U:1U,entry_yaw,1,0);
  assert(status.state==SIGN_ROUTE_ARC && status.entry_line_ready);
  assert(!command.active);
}

static void mode4_drawn_trajectory(int side)
{
  unsigned i;
  int32_t angle;
  int64_t entry_yaw=-side*SIGN_GYRO_TANGENT_ENTRY_MDEG;
  int64_t arc_end_yaw=side*SIGN_GYRO_TANGENT_EXIT_HEADING_MDEG;
  int32_t arc_sweep=SIGN_GYRO_TANGENT_ENTRY_MDEG+
      SIGN_GYRO_TANGENT_EXIT_HEADING_MDEG;
  reach_arc(side);
  assert(status.profile==SIGN_ROUTE_PROFILE_GYRO_TANGENT);

  /* Visible circle line stays under live sensor control. A brief loss keeps
     both wheels forward in the arc direction and never requests a spin. */
  for(angle=10000;angle<arc_sweep;angle+=10000)
  {
    uint8_t mask=angle==70000?0U:6U;
    step(mask,entry_yaw+side*angle,1,angle==20000?1100:0);
    assert(status.state==SIGN_ROUTE_ARC);
    if(mask==0U)
      assert(command.active && command.gentle_arc &&
             command.left_pwm>0 && command.right_pwm>0);
    else assert(!command.active);
  }

  for(i=0;i<4;++i) step(6,arc_end_yaw,1,0);
  if(status.state!=SIGN_ROUTE_EXIT_SELECT)
    fprintf(stderr,"exit trigger side=%d state=%d fault=%u yaw=%ld mm=%ld\n",
        side,(int)status.state,status.fault,(long)status.yaw_mdeg,(long)status.travel_mm);
  assert(status.state==SIGN_ROUTE_EXIT_SELECT && command.active && !command.gentle_arc);
  assert(side<0 ? (command.left_pwm==0 && command.right_pwm>0) :
                  (command.right_pwm==0 && command.left_pwm>0));

  /* Turn back to the approach heading, leave the circle, then drive the second
     blue diagonal until the outgoing centre line is seen again. */
  step(0,side*20000LL,1,0);
  assert(status.state==SIGN_ROUTE_EXIT_SELECT && command.active);
  step(0,0,1,0);
  assert(status.state==SIGN_ROUTE_EXIT_CLEAR && command.active);
  assert(command.left_pwm==command.right_pwm && command.left_pwm>0);
  step(0,0,1,600);
  for(i=0;i<4;++i) step(6,0,1,0);
  assert(status.state==SIGN_ROUTE_LOCKED && !command.active);
}

static void entry_miss_returns_to_heading(int side)
{
  unsigned i;
  int32_t angle;
  int64_t entry_yaw=-side*SIGN_GYRO_TANGENT_ENTRY_MDEG;
  int64_t fallback_arc_end_yaw=side*SIGN_GYRO_TANGENT_EXIT_HEADING_MDEG;
  begin_entry_turn(side);
  step(6,entry_yaw,1,0);
  assert(status.state==SIGN_ROUTE_SELECTING);

  /* About 82 mm of encoder travel with no narrow line must end the diagonal
     probe and start an equal-and-opposite spin back toward the original yaw. */
  step(0,entry_yaw,1,600);
  if(status.state!=SIGN_ROUTE_ENTRY_RETURN)
    fprintf(stderr,"miss fallback side=%d state=%d fault=%u mm=%ld yaw=%ld\n",
        side,(int)status.state,status.fault,(long)status.travel_mm,(long)status.yaw_mdeg);
  assert(status.state==SIGN_ROUTE_ENTRY_RETURN && command.active);
  assert(side<0 ? (command.left_pwm>0 && command.right_pwm<0) :
                  (command.left_pwm<0 && command.right_pwm>0));
  assert(command.left_pwm==-command.right_pwm);

  step(0,0,1,0);
  assert(status.state==SIGN_ROUTE_ENTRY_FALLBACK && command.active);
  assert(command.left_pwm==command.right_pwm && command.left_pwm>0);

  step(6,0,1,300); /* black on the very next sample must not be skipped */
  assert(status.state==SIGN_ROUTE_ARC && status.direction==side &&
         status.entry_line_ready && !command.active);

  /* Every visible mask remains outside route motor ownership in ARC. */
  step(side<0?8U:1U,-side*10000LL,1,0);
  assert(status.state==SIGN_ROUTE_ARC && !command.active);

  /* Sensors acquire the lower biased arc themselves. Exit is based only on
     the signed 60-degree heading from the original observation-stop yaw. */
  for(angle=10000;angle<SIGN_GYRO_TANGENT_EXIT_HEADING_MDEG;angle+=10000)
    step(6,side*angle,1,angle==20000?1100:0);
  for(i=0;i<4;++i) step(6,fallback_arc_end_yaw,1,0);
  assert(status.state==SIGN_ROUTE_EXIT_SELECT && status.direction==side && command.active);
}

static void profile_isolation_and_invalid_gyro(void)
{
  unsigned i;
  now=100U; sequence=0U;
  for(i=0;i<4;++i) counts[i]=0;
  SignRoute_Reset(); SignRoute_SetProfile(SIGN_ROUTE_PROFILE_STANDARD);
  SignRoute_UpdateEncoders(0,0,0,0); SignRoute_UpdateYaw(0,1);
  SignRoute_UpdateObservationPause(1U,now);
  for(i=0;i<3;++i) { observe(-1); step(6,0,1,0); }
  SignRoute_UpdateObservationPause(0U,now); step(6,0,1,0);
  assert(status.state==SIGN_ROUTE_ARMED && !command.active);

  begin_entry_turn(1);
  step(6,0,0,0);
  assert(status.state==SIGN_ROUTE_CANCELLED && !command.active && status.fault==6U);
  puts("PASS: mode 3 ignores pause handoff; invalid mode-4 gyro withdraws without STOP");
}

int main(void)
{
  SignRoute_Init();
  mode4_drawn_trajectory(-1); mode4_drawn_trajectory(1);
  puts("PASS: mirrored fixed-angle turn, straight entry, live arc, turn and straight exit");
  entry_miss_returns_to_heading(-1); entry_miss_returns_to_heading(1);
  puts("PASS: first fallback black enters live ARC immediately and exits at 60 degrees");
  profile_isolation_and_invalid_gyro();
  return 0;
}
