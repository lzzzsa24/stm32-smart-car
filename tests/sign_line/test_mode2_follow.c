/* Actual shared follower/recovery and four-wheel DriveBase; mock only the
   GPIO, clock, encoders, battery and motor pins. Not a physical track model. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "drive_base.h"
#include "wheel_encoder.h"
#include "battery_monitor.h"
#include "motorPWM.h"
#include "sign_line_follow.h"
#include "sign_route_config.h"
#include "sign_observation.h"
#include "line_sensor_sample.h"
#include "line_search_model.h"
#include "line_fault_log.h"
static uint32_t tick, seq;
static int32_t counts[4], fraction[4];
static int16_t pins[4];
static uint8_t gpio_mask;
static uint16_t voltage;
static SignLineFollowController follower;
static SignRouteStatus route;
static SignRouteCommand route_command;
static DriveBaseTelemetry drive;
uint32_t HAL_GetTick(void) { return tick; }
int HAL_GPIO_ReadPin(GPIO_TypeDef *p,uint16_t pin) { (void)p; return gpio_mask & pin ? 0 : 1; }
void HAL_GPIO_Init(GPIO_TypeDef *p,GPIO_InitTypeDef *g) { (void)p; (void)g; }
void HAL_GPIO_WritePin(GPIO_TypeDef *p,uint16_t pin,GPIO_PinState v) { (void)p; (void)pin; (void)v; }
void WheelEncoder_Start(void) {}
void WheelEncoder_GetCounts(WheelEncoderCounts *c)
{ c->motor1=counts[0]; c->motor2=counts[1]; c->motor3=counts[2]; c->motor4=counts[3]; }
void WheelEncoder_GetDiagnostics(WheelEncoderDiagnostics *d) { memset(d,0,sizeof(*d)); }
void BatteryMonitor_Get(BatteryMonitorStatus *b)
{ memset(b,0,sizeof(*b)); b->valid=1; b->millivolts=voltage; }
void DiagnosticUart_WriteString(const char *s) { (void)s; }
void DiagnosticUart_WriteUnsigned(uint32_t v) { (void)v; }
void DiagnosticUart_WriteSigned(int32_t v) { (void)v; }
#define PWM_STUB(n,i) \
 void pwm_motor##n##_forward(int16_t p) { assert(p>=0 && p<=3599); pins[i]=p; } \
 void pwm_motor##n##_backward(int16_t p) { assert(p>=0 && p<=3599); pins[i]=(int16_t)-p; }
PWM_STUB(1,0) PWM_STUB(2,1) PWM_STUB(3,2) PWM_STUB(4,3)
static void set_mask(uint8_t m)
{ gpio_mask=(uint8_t)((m&8?2:0)|(m&4?1:0)|(m&2?4:0)|(m&1?8:0)); }
static void init(uint8_t sign, uint32_t origin)
{
  tick=origin; seq=0; voltage=7800; gpio_mask=0;
  memset(counts,0,sizeof(counts)); memset(fraction,0,sizeof(fraction)); memset(pins,0,sizeof(pins));
  DriveBase_Init(); line_tracking_init(); SignRoute_Init();
  SignRoute_SetProfile(SIGN_ROUTE_PROFILE_STANDARD);
  SignLineFollow_Init(&follower);
  SignObservation_Reset();
  if(sign) SignLineFollow_Start(&follower); else line_tracking_start_following();
}
static uint8_t sample(uint8_t mask,int32_t yaw)
{
  LineTrackingReading reading;
  uint8_t action, paused;
  set_mask(mask); tick+=10U; LineSensorSample_Tick(tick);
  reading=line_tracking_read();
  SignRoute_UpdateEncoders(counts[0],counts[1],counts[2],counts[3]);
  SignRoute_UpdateYaw(3700000LL+yaw,1);
  SimpleLine_UpdateYaw(&follower.guard,3700000LL+yaw,1,1);
  SignRoute_GetStatus(tick,&route);
  if(route.profile==SIGN_ROUTE_PROFILE_STANDARD)
  {
    SignObservation_UpdateLine(mask,tick);
    if(SignObservation_SeekingLine()) SignRoute_MarkObservationSearch();
  }
  paused=SignObservation_Paused(tick);
  SignRoute_UpdateObservationPause(route.profile==SIGN_ROUTE_PROFILE_STANDARD ?
      SignObservation_HoldingRoute(tick) : paused,tick);
  SignRoute_Step(mask,tick,&route_command); SignRoute_GetStatus(tick,&route);
  action=SignLineFollow_Step(&follower,&reading,3000,&route,&route_command,paused);
  DriveBase_Task(tick); DriveBase_GetTelemetry(&drive);
  return action;
}
/* Former completion fixtures now explicitly provide the user-selected
   outer clear/black event; production no longer completes on the middle pair. */
static void finish_outer(int side)
{
  int32_t yaw=(int32_t)(follower.guard.yaw_mdeg-3700000LL);
  if(route.state==SIGN_ROUTE_EXIT_SELECT || route.state==SIGN_ROUTE_EXIT_CLEAR)
  {
    sample(6,yaw);
    sample(side<0?14:7,yaw);
    sample(6,yaw);
  }
}
static void observe_score(int side, uint8_t score)
{
  VisionDetection d={0}; d.class_id=side<0?0:1; d.score=score;
  d.center_x=160; d.center_y=120; d.sequence=++seq; d.received_ms=tick;
  SignRoute_GetStatus(tick,&route);
  SignObservation_AllowPause(route.direction==0 && (route.state==SIGN_ROUTE_IDLE ||
      route.state==SIGN_ROUTE_ARMED || route.state==SIGN_ROUTE_PROBE || route.state==SIGN_ROUTE_WAIT_SIGN));
  SignObservation_ObserveDetection(&d,tick,
      route.profile==SIGN_ROUTE_PROFILE_STANDARD ? SIGN_OBSERVATION_MODE3_SCORE_MINIMUM :
                                                  SIGN_OBSERVATION_MODE4_SCORE_MINIMUM);
  SignRoute_ObserveDetection(&d);
}
static void observe(int side) { observe_score(side,80); }

static void weak_votes_must_not_skip_observation(int side, uint32_t origin)
{
  unsigned i,w;
  uint32_t stopped;
  init(1,origin);
  for(i=0;i<3;++i)
  {
    observe_score(side,19);
    sample(6,0);
  }
  /* Real application inhibits parking as soon as route.direction is set.
     Weak pre-stop votes therefore must not publish a committed direction. */
  if(route.direction) fprintf(stderr,"weak votes prematurely inhibited observation: side=%d state=%d direction=%d\n",side,route.state,route.direction);
  assert(route.direction==0 && !SignObservation_Paused(tick));
  stopped=tick;
  observe_score(side,20);
  assert(sample(6,0)==6U && route.direction==0);
  for(i=0;i<198;++i)
  {
    observe_score(side,20);
    assert(sample(6,0)==6U);
    for(w=0;w<4;++w) assert(drive.requested_cps[w]==0);
  }
  assert(tick-stopped==1990U);
  observe_score(side,20);
  sample(6,0);
  assert(!SignObservation_Paused(tick) && route.direction==side);
  assert(route.approach_from_pause && route.road_reference_valid);
  for(i=0;i<80;++i) { observe_score(side,20); assert(sample(6,0)!=6U); }
  puts("PASS: 19-percent rejected then first 20-percent frame stops for full 2s, votes confirm direction, retain stopped heading and do not repeat confirmed stop");
}
static void drive_before_stop(int32_t yaw)
{
  unsigned i,w;
  for(i=0;i<45;++i)
  {
    for(w=0;w<4;++w) counts[w]+=12;
    sample(6,yaw);
  }
  assert(!route.road_reference_valid); /* travel alone no longer chooses the heading */
}
static uint8_t normalized(LineTrackingAction a)
{
  if(a==LINE_ACTION_STOP)return 0;
  if(a==LINE_ACTION_CROSSING)return 4;
  if(a==LINE_ACTION_SEARCH_LEFT||a==LINE_ACTION_SEARCH_RIGHT)return 3;
  if(a==LINE_ACTION_LEFT_SHARP||a==LINE_ACTION_RIGHT_SHARP)return 2;
  return 1;
}
static void parity(uint32_t origin)
{
  static struct { int32_t request[4],control[4]; int16_t pwm[4]; uint8_t action; } baseline[2000];
  unsigned pass,i,j,w;
  const uint8_t patterns[]={0,6,4,6,2,6,8,8,12,0,6,1,1,3,0,6,15,0,7,14,5,10,9,11,13};
  const unsigned dt[]={1,2,10,20,40,80};
  for(pass=0;pass<4;++pass) /* actual KEY2, then mode3 approach/ARC/EXIT LINE */
  {
    init(pass!=0,origin);
    if(!pass) line_tracking_set_middle_guard(1U);
    for(i=0;i<2000;++i)
    {
      uint8_t mask=patterns[(i/17) % sizeof(patterns)],action;
      LineTrackingReading reading;
      set_mask(mask);
      voltage=i<700?8400:(i<1400?7400:7000);
      DriveBase_GetTelemetry(&drive);
      for(j=0;j<dt[(i/9)%6];++j)
      {
        ++tick;
        for(w=0;w<4;++w)
        { fraction[w]+=drive.requested_cps[w]/2; counts[w]+=fraction[w]/1000; fraction[w]%=1000; }
        LineSensorSample_Tick(tick);
      }
      if(!pass)
      {
        LineTrackingCommand normal;
        LineTrackingAction a;
        reading=line_tracking_read();
        a=line_tracking_compute(&reading,3000,&normal);
        line_tracking_apply_command(&normal,3599);
        action=normalized(a);
      }
      else
      {
        reading=line_tracking_read();
        SignRoute_UpdateYaw(0,1);
        SignRoute_UpdateEncoders(counts[0],counts[1],counts[2],counts[3]);
        SignRoute_Step(mask,tick,&route_command); SignRoute_GetStatus(tick,&route);
        if(pass>=2)
        {
          route.state=pass==2?SIGN_ROUTE_ARC:SIGN_ROUTE_EXIT_CLEAR;
          route.direction=1; route_command.active=0;
        }
        SimpleLine_UpdateYaw(&follower.guard,0,1,1);
        /* Exercise the actual sign owner with no temporary caps. */
        action=SignLineFollow_Step(&follower,&reading,3000,&route,&route_command,0);
      }
      DriveBase_Task(tick); DriveBase_GetTelemetry(&drive);
      for(w=0;w<4;++w)
      {
        if(!pass) { baseline[i].request[w]=drive.requested_cps[w]; baseline[i].control[w]=drive.controlled_cps[w]; baseline[i].pwm[w]=pins[w]; }
        else { assert(baseline[i].request[w]==drive.requested_cps[w]); assert(baseline[i].control[w]==drive.controlled_cps[w]); assert(baseline[i].pwm[w]==pins[w]); }
      }
      if(!pass)baseline[i].action=action; else assert(baseline[i].action==action);
    }
  }
  puts("PASS: 2000 samples per mode3 approach/ARC/EXIT LINE match actual mode2 middle guard, all requested/controlled/PWM outputs and actions");
}
static uint8_t key2_step(uint8_t mask)
{
  LineTrackingReading reading;
  LineTrackingCommand command;
  LineTrackingAction action;
  set_mask(mask); tick+=10U; LineSensorSample_Tick(tick);
  reading=line_tracking_read();
  action=line_tracking_compute(&reading,3000,&command);
  line_tracking_apply_command(&command,3599);
  DriveBase_Task(tick); DriveBase_GetTelemetry(&drive);
  return normalized(action);
}
static void slow_profile_matches_key2_settle(void)
{
  unsigned mask,i,w;
  int32_t expected[4];
  uint8_t action;
  for(mask=0;mask<16;++mask)
  {
    /* Actual KEY2 middle guard is the oracle. The same first contact
       precedes each mask. */
    init(0,100); line_tracking_set_middle_guard(1U);
    key2_step(6); action=key2_step((uint8_t)mask);
    memcpy(expected,drive.requested_cps,sizeof(expected));
    init(1,100); sample(6,0);
    assert(sample((uint8_t)mask,0)==action);
    for(w=0;w<4;++w) assert(drive.requested_cps[w]==expected[w]);
  }
  init(1,100);
  for(i=0;i<3000;++i)
  {
    sample(6,0);
    for(w=0;w<4;++w) assert(drive.requested_cps[w]==LINE_TRACKING_MIDDLE_GUARD_CPS);
  }
  /* A fresh generic follower must clear the mode-specific middle guard. */
  SignLineFollow_Stop(&follower); line_tracking_start_following();
  for(i=0;i<100;++i) key2_step(6);
  for(w=0;w<4;++w)
    assert(drive.requested_cps[w]==DriveBase_EquivalentCpsFromPwm(2700));
  puts("PASS: all 16 masks match KEY2 middle guard, 30s steady speed, generic restart clears profile");
}
static void slow_speed_and_recognition(void)
{
  unsigned i;
  uint32_t pause_start;
  const int32_t cruise=LINE_TRACKING_MIDDLE_GUARD_CPS;
  init(1,100);
  sample(6,0); assert(drive.requested_cps[0]==cruise);
  pause_start=tick;
  /* Retain exactly two seconds of observation, then restore the slow profile
     with no post-pause speed cap or repeat stop after direction confirmation. */
  for(i=0;i<300;++i)
  {
    observe(1); sample(6,0);
    if(tick-pause_start<2000U)
      assert(!drive.requested_cps[0]&&!drive.requested_cps[2]);
    else assert(drive.requested_cps[0]==cruise&&drive.requested_cps[2]==cruise);
  }
  assert(route.direction==1);
  for(i=0;i<100;++i)
  {
    sample(15,0);
    assert(drive.requested_cps[0]==cruise&&drive.requested_cps[2]==cruise);
  }
  for(i=0;i<40;++i) sample(0,0);
  assert(drive.requested_cps[0]==-drive.requested_cps[2]);
  assert(follower.last_owner==SIGN_FOLLOW_OWNER_GUARD); /* sign selection still constrains entry */
  assert(drive.requested_cps[0]==LINE_SEARCH_TARGET_CPS||drive.requested_cps[0]==-LINE_SEARCH_TARGET_CPS);
  observe(1);
  sample(0,0);
  assert(drive.requested_cps[0]==-drive.requested_cps[2]&&drive.requested_cps[0]!=0);
  SignLineFollow_Stop(&follower); sample(0,0);
  for(i=0;i<4;++i)assert(!pins[i]&&!drive.requested_cps[i]);
  puts("PASS: fixed 2-second observation, slow-speed resume, no repeated confirmed stop, same all-black speed and STOP");
}
static void ring(int side,uint32_t origin)
{
  unsigned i,w; int angle; uint8_t selected=side<0?8:1,opposite=side<0?1:8;
  init(1,origin);
  drive_before_stop(0);
  /* Camera and control loop keep running throughout the observation stop. */
  for(i=0;i<203;++i) { observe(side); sample(6,0); }
  for(i=0;i<3;++i) sample(15,0);
  assert(route.state==SIGN_ROUTE_PROBE);
  sample(opposite,0);
  assert(side<0?drive.requested_cps[0]<0:drive.requested_cps[2]<0);
  assert(drive.requested_cps[0]==-drive.requested_cps[2]);
  sample(selected,-side*10000); assert(route_command.active);
  assert(side<0?drive.requested_cps[0]==0:drive.requested_cps[2]==0);
  assert(drive.requested_cps[0]+drive.requested_cps[2]==2200); /* KEY2 normal outer-pivot target */
  for(i=0;i<4;++i)sample(6,-side*20000);
  assert(route.state==SIGN_ROUTE_ARC && route.entry_line_ready);
  sample(0,-side*20000);
  assert(!follower.override_active && drive.requested_cps[0]==-1800); /* mode2 fallback without a lateral hint */
  for(angle=21;angle<=80;++angle)
  {
    for(w=0;w<4;++w)counts[w]+=22;
    tick+=40; sample(selected,-side*angle*1000);
    assert(route.state==SIGN_ROUTE_ARC && !route_command.active);
    assert(drive.requested_cps[0]==-1800); /* same persistent mode2 recovery begun at the preceding white sample */
    assert(drive.requested_cps[2]==-drive.requested_cps[0]);
  }
  for(angle=1;angle<=171;++angle)
  {
    for(w=0;w<4;++w)counts[w]+=22;
    tick+=40; sample(6,side*(angle-80)*1000);
    if(route.state==SIGN_ROUTE_EXIT_SELECT) break;
  }
  assert(route.state==SIGN_ROUTE_EXIT_SELECT && route_command.active); /* qualified exit owns heading until aligned */
  assert(angle-80<=66); /* start turning before the far junction's 90-degree tangent */
  for(angle=angle-80;angle>=26;--angle)
  {
    tick+=40; sample(0,side*angle*1000);
    assert(route.state==SIGN_ROUTE_EXIT_SELECT);
    assert(side<0?drive.requested_cps[0]==0:drive.requested_cps[2]==0);
  }
  tick+=90; sample(0,-side*12000);
  assert(route.state==SIGN_ROUTE_EXIT_CLEAR);
  for(i=0;i<20;++i)
  {
    for(w=0;w<4;++w)counts[w]+=22;
    tick+=40; sample(0,-side*12000);
    assert(route.state==SIGN_ROUTE_EXIT_CLEAR);
    assert(!route_command.active && !follower.override_active);
    assert(drive.requested_cps[0]!=0&&drive.requested_cps[0]==-drive.requested_cps[2]);
  }
  for(i=0;i<2;++i){tick+=40;sample(6,-side*12000);}
  finish_outer(side);  assert(route.state==SIGN_ROUTE_LOCKED);
  sample(6,-side*12000); assert(drive.requested_cps[0]>1200);
  SignLineFollow_Stop(&follower); sample(selected,0);
  for(w=0;w<4;++w)assert(!pins[w]&&!drive.requested_cps[w]);
  puts("PASS: real route -> KEY2 follower -> DriveBase, selected branch, ARC recovery, gyro exit, rejoin and STOP");
}
static void late_choice(int side)
{
  unsigned i;
  init(1,100);
  for(i=0;i<3;++i)sample(15,0);
  for(i=0;i<40;++i)sample(0,0);
  assert(route.state==SIGN_ROUTE_WAIT_SIGN);
  for(i=0;i<203;++i){observe(side);sample(0,0);}
  assert(SignObservation_HoldingRoute(tick) && route.direction==side);
  for(i=0;i<204;++i){observe(side);sample(6,0);}
  assert(!SignObservation_HoldingRoute(tick) && route.direction==side);
  for(i=0;i<3;++i)sample(15,0);
  sample(0,0);
  assert(route.state==SIGN_ROUTE_PROBE);
  assert(side<0?drive.requested_cps[0]<0:drive.requested_cps[2]<0);
  sample(side<0?1:8,0); /* opposite evidence cannot take back ownership */
  assert(side<0?drive.requested_cps[0]<0:drive.requested_cps[2]<0);
  sample(side<0?8:1,-side*10000);
  assert(route_command.active);
  assert(drive.requested_cps[0]+drive.requested_cps[2]==2200);
  sample(side<0?8:1,-side*10000);
  assert(side<0?drive.requested_cps[0]==0:drive.requested_cps[2]==0);
  assert(drive.requested_cps[0]+drive.requested_cps[2]>0);
  /* Route cancellation hands back a live KEY2 follower, not stale search. */
  tick+=10001;sample(6,0);
  assert(route.state==SIGN_ROUTE_CANCELLED);
  assert(drive.requested_cps[0]>0&&drive.requested_cps[2]>0);
  puts("PASS: late confirmed choice preempts shared search at normal turn speed; cancel resumes live line");
}
static void mode4_drawn_drive(int side)
{
  unsigned i,w;
  const int32_t inner=DriveBase_EquivalentCpsFromPwm(SIGN_GYRO_TANGENT_INNER_PWM);
  const int32_t outer=DriveBase_EquivalentCpsFromPwm(SIGN_GYRO_TANGENT_OUTER_PWM);
  init(1,100); SignRoute_SetProfile(SIGN_ROUTE_PROFILE_GYRO_TANGENT);
  for(i=0;i<203;++i) { observe(side); sample(6,0); }
  assert(route.state==SIGN_ROUTE_PROBE && route_command.active && !route_command.gentle_arc);
  assert(side<0 ? drive.requested_cps[0]<0 && drive.requested_cps[2]>0 :
                  drive.requested_cps[2]<0 && drive.requested_cps[0]>0);
  assert(drive.requested_cps[0]==-drive.requested_cps[2]);
  sample(6,-side*SIGN_GYRO_TANGENT_ENTRY_MDEG);
  assert(route.state==SIGN_ROUTE_SELECTING && route_command.active);
  assert(drive.requested_cps[0]==drive.requested_cps[2] && drive.requested_cps[0]>0);
  for(w=0;w<4;++w) counts[w]+=400;
  sample(0,-side*SIGN_GYRO_TANGENT_ENTRY_MDEG);
  for(i=0;i<4;++i) sample(side<0?8:1,-side*SIGN_GYRO_TANGENT_ENTRY_MDEG);
  assert(route.state==SIGN_ROUTE_ARC);
  sample(side<0?8:1,-side*SIGN_GYRO_TANGENT_ENTRY_MDEG);
  assert(!route_command.active);
  assert(!follower.override_active && follower.last_owner==SIGN_FOLLOW_OWNER_LINE);
  assert(side<0 ? drive.requested_cps[0]==0 && drive.requested_cps[2]==2200 :
                  drive.requested_cps[2]==0 && drive.requested_cps[0]==2200);
  sample(6,-side*SIGN_GYRO_TANGENT_ENTRY_MDEG);
  assert(!route_command.active && drive.requested_cps[0]==drive.requested_cps[2]);
  sample(14,-side*SIGN_GYRO_TANGENT_ENTRY_MDEG);
  assert(!route_command.active && drive.requested_cps[0]<drive.requested_cps[2]);
  sample(7,-side*SIGN_GYRO_TANGENT_ENTRY_MDEG);
  assert(!route_command.active && drive.requested_cps[0]>drive.requested_cps[2]);
  sample(15,-side*SIGN_GYRO_TANGENT_ENTRY_MDEG);
  assert(!route_command.active && !follower.override_active &&
         follower.last_owner==SIGN_FOLLOW_OWNER_ARC_FALLBACK);
  assert(drive.requested_cps[0]>0 && drive.requested_cps[2]>0 &&
         drive.requested_cps[0]!=drive.requested_cps[2]);
  sample(0,-side*SIGN_GYRO_TANGENT_ENTRY_MDEG+side*10000L);
  assert(!route_command.active && !follower.override_active &&
         follower.last_owner==SIGN_FOLLOW_OWNER_ARC_FALLBACK);
  assert(drive.requested_cps[0]==outer && drive.requested_cps[2]==inner);
  assert(drive.requested_cps[0]>0 && drive.requested_cps[2]>0 &&
         drive.requested_cps[0]!=drive.requested_cps[2]);
  /* Repeated white/black transitions keep the same ARC owner and immediately
     accept the current edge instead of resetting into a second controller. */
  for(i=0;i<20;++i)
  {
    uint8_t current=(i&1U)?0U:(side<0?8U:1U);
    sample(current,-side*SIGN_GYRO_TANGENT_ENTRY_MDEG+side*10000L);
    assert(!route_command.active && !follower.override_active);
    assert(follower.last_owner==(current?SIGN_FOLLOW_OWNER_LINE:
                                         SIGN_FOLLOW_OWNER_ARC_FALLBACK));
    assert(drive.requested_cps[0]>=0 && drive.requested_cps[2]>=0);
  }
  for(i=0;i<100;++i)
  {
    sample(0,-side*SIGN_GYRO_TANGENT_ENTRY_MDEG+side*10000L);
    assert(!route_command.active && !follower.override_active &&
           follower.last_owner==SIGN_FOLLOW_OWNER_ARC_FALLBACK);
  }
  sample(side<0?8U:1U,-side*SIGN_GYRO_TANGENT_ENTRY_MDEG+side*10000L);
  assert(follower.last_owner==SIGN_FOLLOW_OWNER_LINE &&
         drive.requested_cps[0]>=0 && drive.requested_cps[2]>=0);
  SignLineFollow_Stop(&follower); sample(0,0);
  for(w=0;w<4;++w) assert(!pins[w]&&!drive.requested_cps[w]);
}
static void enter_test_arc(int side, uint32_t origin)
{
  unsigned i;
  init(1,origin);
  drive_before_stop(0);
  for(i=0;i<203;++i) { observe(side); sample(6,0); }
  for(i=0;i<3;++i) sample(15,0);
  sample(side<0?8:1,-side*20000);
  for(i=0;i<4;++i) sample(6,-side*20000);
  assert(route.state==SIGN_ROUTE_ARC && route.entry_line_ready);
}
static void arc_feedback_and_entry_search(void)
{
  unsigned i,w,failures=0;
  int side,angle;
  for(side=-1;side<=1;side+=2)
  {
    uint8_t opposite=side<0?1:8;
    enter_test_arc(side,100);
    sample(15,-side*20000);
    sample(opposite,-side*20000);
    if (!(side<0 ? drive.requested_cps[0]==1800 && drive.requested_cps[2]==-1800 :
                  drive.requested_cps[0]==-1800 && drive.requested_cps[2]==1800))
    {
      fprintf(stderr,"ARC bar->outer lost feedback side=%d targets=%ld/%ld\n",
          side,(long)drive.requested_cps[0],(long)drive.requested_cps[2]); ++failures;
    }
    assert(route.state==SIGN_ROUTE_ARC && !route_command.active);

    /* ARC loss uses the same mode2 direction evidence as ordinary tracking.
       With no lateral line hint, gyro trend cannot install another search owner. */
    enter_test_arc(side,UINT32_MAX-120U);
    for(angle=1;angle<=6;++angle) sample(6,side*(angle-20)*1000);
    sample(0,-side*14000);
    if (drive.requested_cps[0]!=-1800 || follower.override_active)
    {
      fprintf(stderr,"ARC loss bypassed mode2 direction rules side=%d targets=%ld/%ld\n",
          side,(long)drive.requested_cps[0],(long)drive.requested_cps[2]); ++failures;
    }

    init(1,100);
    for(i=0;i<203;++i) { observe(side); sample(6,0); }
    for(i=0;i<3;++i) sample(15,0);
    for(angle=0;angle<=100;++angle)
    {
      sample(0,-side*angle*1000);
      if (!(side<0 ? drive.requested_cps[0]<0 : drive.requested_cps[2]<0))
      {
        fprintf(stderr,"Entry search reversed before corner side=%d at %d degrees\n",side,angle);
        ++failures; break;
      }
    }
    SignLineFollow_Stop(&follower); sample(0,0);
    for(w=0;w<4;++w) assert(!pins[w]&&!drive.requested_cps[w]);
  }
  assert(failures==0);
  puts("PASS: ARC uses KEY2 edge/white search; pre-entry direction guard and STOP retained");
}
static void exit_releases_direction(int side, uint32_t origin)
{
  unsigned i,w;
  uint8_t opposite=side<0?1:8;
  enter_test_arc(side,origin);
  sample(6,-side*80000); /* entry apex */
  for(w=0;w<4;++w) counts[w]+=2000;
  sample(6,side*80000); /* one upper-half sample cannot complete turn debounce */
  for(i=0;i<25;++i)
  { for(w=0;w<4;++w) counts[w]+=22; sample(6,0); } /* actual outgoing travel */
  finish_outer(side);  assert(route.state==SIGN_ROUTE_LOCKED && route.direction==0);
  assert(!route_command.active);
  assert(!follower.override_active && !follower.guard.entry_guard_active);
  assert(!follower.guard.route_hint && !follower.guard.curve_yaw_valid);
  for(w=0;w<4;++w) assert(drive.requested_cps[w]==LINE_TRACKING_MIDDLE_GUARD_CPS);

  /* A new bend on the ordinary line points opposite to the old sign. Its
     current sensor contact, not the completed route, must choose the wheels. */
  for(i=0;i<6;++i)
  {
    sample(opposite,0);
    assert(!route_command.active && route.direction==0);
    assert(side<0 ? drive.requested_cps[0]==1800 && drive.requested_cps[2]==-1800 :
                   drive.requested_cps[0]==-1800 && drive.requested_cps[2]==1800);
  }
  for(i=0;i<40;++i) sample(0,side*100000);
  finish_outer(side);  assert(route.state==SIGN_ROUTE_LOCKED && route.direction==0 && !route_command.active);
  assert(!follower.override_active); /* old ARC's +/-25-degree sector cannot intervene */
  assert(side<0 ? drive.requested_cps[0]==LINE_TRACKING_MIDDLE_GUARD_CPS :
                 drive.requested_cps[0]==-LINE_TRACKING_MIDDLE_GUARD_CPS);
  assert(drive.requested_cps[0]==-drive.requested_cps[2]);
  for(i=0;i<10;++i) sample(6,0);
  for(w=0;w<4;++w) assert(drive.requested_cps[w]==LINE_TRACKING_MIDDLE_GUARD_CPS);

  /* Active exit alignment also relinquishes ownership at the FIRST real
     contact, including a contact in the alignment-to-clear transition. */
  enter_test_arc(side,origin);
  sample(6,-side*80000);
  for(w=0;w<4;++w) counts[w]+=2000;
  for(i=0;i<4;++i) sample(6,side*90000);
  assert(route.state==SIGN_ROUTE_EXIT_SELECT);
  sample(0,side*30000); assert(route_command.active);
  sample(opposite,side*5000);
  assert(route.state==SIGN_ROUTE_EXIT_CLEAR && !route_command.active);
  for(i=0;i<40;++i)
  {
    sample(0,side*5000);
    assert(!route_command.active && !follower.override_active);
  }
  assert(drive.requested_cps[0]==-drive.requested_cps[2] && drive.requested_cps[0]!=0);
  /* No extra 60 mm of straight motion may keep R alive after capture. */
  for(i=0;i<4;++i) sample(6,side*5000);
  finish_outer(side);  assert(route.state==SIGN_ROUTE_LOCKED && route.direction==0 && !route_command.active);
  for(w=0;w<4;++w) assert(drive.requested_cps[w]==LINE_TRACKING_MIDDLE_GUARD_CPS);
  SignLineFollow_Stop(&follower); sample(0,side*100000);
  for(w=0;w<4;++w) assert(!pins[w]&&!drive.requested_cps[w]);
  puts("PASS: exit clears L/R and route motor ownership; opposite bend/re-loss use shared follower; no delayed exit turn");
}
static void continuous_line_exit(int side, uint32_t origin, uint8_t early_edge)
{
  unsigned i,w;
  int angle, start_heading=early_edge?30000:40000;
  uint8_t selected=side<0?8:1;
  enter_test_arc(side,origin);
  sample(6,-side*80000);
  for(w=0;w<4;++w) counts[w]+=2000;
  /* An outside contact before the exit region is still ordinary arc tracking. */
  for(i=0;i<4;++i) sample(selected,side*29000);
  assert(route.state==SIGN_ROUTE_ARC && !route_command.active);
  for(i=0;i<4;++i) sample(early_edge?selected:6,side*start_heading);
  assert(route.state==SIGN_ROUTE_EXIT_SELECT && route_command.active);
  /* Continuous ring line cannot revoke an unfinished, angle-qualified exit.
     A fixed heading cannot falsely finish it just because the middle is black. */
  for(i=0;i<10;++i)
  {
    sample(6,side*start_heading);
    assert(route.state==SIGN_ROUTE_EXIT_SELECT && route_command.active);
    assert(side<0 ? drive.requested_cps[0]==0 && drive.requested_cps[2]==2200 :
                   drive.requested_cps[0]==2200 && drive.requested_cps[2]==0);
  }
  sample((uint8_t)(15U ^ selected),side*start_heading); assert(!route_command.active);
  sample((uint8_t)(15U ^ selected),side*start_heading); assert(!route_command.active);
  for(angle=start_heading-5000;angle>25000;angle-=5000)
  {
    sample(6,side*angle);
    assert(route_command.active && route.state==SIGN_ROUTE_EXIT_SELECT);
  }
  for(i=0;i<5;++i) sample(6,side*10000);
  finish_outer(side);  assert(route.state==SIGN_ROUTE_LOCKED && route.direction==0 && !route_command.active);
  for(w=0;w<4;++w) assert(drive.requested_cps[w]==LINE_TRACKING_MIDDLE_GUARD_CPS);
  for(i=0;i<40;++i) sample(0,side*120000);
  finish_outer(side);  assert(route.state==SIGN_ROUTE_LOCKED && !route_command.active && !follower.override_active);
  SignLineFollow_Stop(&follower); sample(0,0);
  for(w=0;w<4;++w) assert(!pins[w]&&!drive.requested_cps[w]);
  puts("PASS: continuous ring line cannot cancel the exit choice; aligned outgoing line releases it; no renewed lap command");
}
static void paused_heading_reference(int side, uint32_t origin)
{
  unsigned i,w;
  const int32_t stopped_yaw=side*10000;
  init(1,origin);
  drive_before_stop(stopped_yaw);
  for(i=0;i<203;++i) { observe(side); sample(6,stopped_yaw); }
  assert(route.road_reference_valid && route.approach_from_pause && route.heading_error_mdeg==0);
  /* The approach can need a small correction between the observation stop
     and the crossbar. That must not silently redefine the outgoing heading. */
  for(i=0;i<3;++i) sample(15,stopped_yaw+side*20000);
  assert(route.state==SIGN_ROUTE_PROBE && route.heading_error_mdeg==side*20000);
  assert(route.road_reference_valid && route.approach_from_pause);
  sample(side<0?8:1,stopped_yaw-side*20000);
  for(i=0;i<4;++i) sample(6,stopped_yaw-side*20000);
  assert(route.state==SIGN_ROUTE_ARC);
  sample(6,stopped_yaw-side*80000);
  for(w=0;w<4;++w) counts[w]+=2000;
  for(i=0;i<25;++i)
  { for(w=0;w<4;++w) counts[w]+=22; sample(6,stopped_yaw); }
  assert(route.state==SIGN_ROUTE_ARC && !route_command.active); /* midpoint also faces zero */
  sample(6,stopped_yaw+side*80000); /* natural rejoin before turn debounce completes */
  assert(route.state==SIGN_ROUTE_ARC);
  for(i=0;i<25;++i)
  { for(w=0;w<4;++w) counts[w]+=22; sample(6,stopped_yaw); }
  finish_outer(side);  assert(route.state==SIGN_ROUTE_LOCKED && route.direction==0 && route.heading_error_mdeg==0);
  for(w=0;w<4;++w) assert(drive.requested_cps[w]==LINE_TRACKING_MIDDLE_GUARD_CPS);
  puts("PASS: stopped reference survives later approach correction; midpoint zero does not end the arc");
}
static void naturally_departed_before_gate(int side, uint32_t origin, int32_t outgoing)
{
  unsigned i,w;
  enter_test_arc(side,origin);
  sample(6,-side*80000);
  for(w=0;w<4;++w) counts[w]+=2000;
  for(i=0;i<20;++i)
  {
    for(w=0;w<4;++w) counts[w]+=22;
    sample(6,0);
    assert(route.state==SIGN_ROUTE_ARC); /* the midpoint has not passed the upper half */
  }
  sample(6,side*55000); /* this run never reaches the old 70-degree flag */
  for(i=0;i<25;++i)
  {
    for(w=0;w<4;++w) counts[w]+=22;
    sample(i%4==0?4:6,side*outgoing);
    assert(!route_command.active);
  }
  finish_outer(side);  assert(route.state==SIGN_ROUTE_LOCKED && route.direction==0 && !follower.override_active);
  sample(6,side*outgoing);
  for(w=0;w<4;++w) assert(drive.requested_cps[w]==LINE_TRACKING_MIDDLE_GUARD_CPS);
  sample(side<0?1:8,side*outgoing);
  for(i=0;i<40;++i) sample(0,side*100000);
  finish_outer(side);  assert(route.state==SIGN_ROUTE_LOCKED && !route_command.active && !follower.override_active);
  SignLineFollow_Stop(&follower); sample(0,0);
  for(w=0;w<4;++w) assert(!drive.requested_cps[w]&&!pins[w]);
  puts("PASS: upper-half peak below 70 and a slightly oblique outgoing line still complete ARC and release old direction");
}
static void departure_evidence_is_bounded(int side)
{
  unsigned i,w;
  uint8_t selected=side<0?8:1,opposite=side<0?1:8;
  enter_test_arc(side,100);
  sample(6,-side*80000);
  for(w=0;w<4;++w) counts[w]+=2000;
  sample(6,side*55000);
  /* Waiting with centered sensors and zero wheel travel is not an exit. */
  for(i=0;i<30;++i) sample(6,0);
  assert(route.state==SIGN_ROUTE_ARC && !route_command.active);
  /* A return may withdraw ARC authority, but a changing tangent is not
     confirmed outgoing road and cannot restore the old forced turn. */
  for(i=0;i<20;++i)
  {
    for(w=0;w<4;++w) counts[w]+=22;
    sample(6,side*(25000-(int32_t)i*1000));
    assert(route.state!=SIGN_ROUTE_LOCKED && route.state!=SIGN_ROUTE_EXIT_SELECT && !route_command.active);
  }
  /* Other sensors cannot finish without the selected outer. */
  for(i=0;i<30;++i)
  {
    for(w=0;w<4;++w) counts[w]+=22;
    sample(i%3==0?0:(i%3==1?opposite:(uint8_t)(15U ^ selected)),0);
    assert(route.state!=SIGN_ROUTE_LOCKED && route.state!=SIGN_ROUTE_EXIT_SELECT && !route_command.active);
  }
  for(i=0;i<8;++i)
  { for(w=0;w<4;++w) counts[w]+=22; sample(6,0); }
  tick+=60; sample(6,0);
  assert(route.state!=SIGN_ROUTE_LOCKED && !route_command.active); /* gap cannot confirm */
  for(i=0;i<8;++i)
  { for(w=0;w<4;++w) counts[w]+=22; sample(6,0); }
  assert(route.state!=SIGN_ROUTE_LOCKED && !route_command.active);

  /* Between the new 30/40-degree gates, only the outward contact selects. */
  enter_test_arc(side,100);
  sample(6,-side*80000);
  for(w=0;w<4;++w) counts[w]+=2000;
  for(i=0;i<4;++i) sample(selected,side*29000);
  assert(route.state==SIGN_ROUTE_ARC && !route_command.active);
  for(i=0;i<4;++i) sample(opposite,side*35000);
  assert(route.state==SIGN_ROUTE_ARC && !route_command.active);
  for(i=0;i<4;++i) sample(selected,side*35000);
  assert(route.state==SIGN_ROUTE_EXIT_SELECT && route_command.active);
  assert(side<0 ? drive.requested_cps[0]==0 && drive.requested_cps[2]==2200 :
                 drive.requested_cps[0]==2200 && drive.requested_cps[2]==0);
  for(i=0;i<5;++i) sample(6,0);
  finish_outer(side);  assert(route.state==SIGN_ROUTE_LOCKED && route.direction==0 && !route_command.active);
  SignLineFollow_Stop(&follower); sample(0,0);
  for(w=0;w<4;++w) assert(!drive.requested_cps[w]&&!pins[w]);
  puts("PASS: no stationary/curving/ambiguous/gapped false completion; 35-degree contact selects only the outward edge");
}
static void earlier_exit_timing(int side, uint32_t origin, uint8_t edge_contact)
{
  unsigned i,w;
  int angle, heading=edge_contact?30000:40000;
  uint8_t mask=edge_contact?(side<0?8:1):6;
  enter_test_arc(side,origin);
  sample(6,-side*80000);
  for(w=0;w<4;++w) counts[w]+=2000;
  /* The same magnitude in the lower half, and zero at the midpoint, cannot
     become an exit. Only the upper-half signed heading advances the turn. */
  for(i=0;i<4;++i) sample(mask,-side*heading);
  assert(route.state==SIGN_ROUTE_ARC && !route_command.active);
  for(i=0;i<4;++i) sample(mask,0);
  assert(route.state==SIGN_ROUTE_ARC && !route_command.active);
  for(i=0;i<4;++i) sample(mask,side*(heading-1000));
  assert(route.state==SIGN_ROUTE_ARC && !route_command.active);
  /* At the earlier heading, ambiguous input still cannot start a turn, and
     one valid frame followed by a wide mark cannot bypass the 30-ms filter. */
  for(i=0;i<12;++i) sample(i%3==0?0:(i%3==1?9:15),side*heading);
  assert(route.state==SIGN_ROUTE_ARC && !route_command.active);
  sample(mask,side*heading);
  sample(15,side*heading);
  for(i=0;i<3;++i) sample(mask,side*heading);
  assert(route.state==SIGN_ROUTE_ARC && !route_command.active);
  sample(mask,side*heading);
  assert(route.state==SIGN_ROUTE_EXIT_SELECT && route_command.active);
  assert(side<0 ? drive.requested_cps[0]==0 && drive.requested_cps[2]==2200 :
                 drive.requested_cps[0]==2200 && drive.requested_cps[2]==0);
  /* Follow the commanded alignment, not another 25 degrees around the ring.
     The original stopped heading and ordinary forward target remain intact. */
  for(angle=heading-5000;angle>25000;angle-=5000) sample(6,side*angle);
  for(i=0;i<5;++i) sample(6,side*10000);
  finish_outer(side);  assert(route.state==SIGN_ROUTE_LOCKED && route.direction==0 && !route_command.active);
  for(w=0;w<4;++w) assert(drive.requested_cps[w]==LINE_TRACKING_MIDDLE_GUARD_CPS);
  SignLineFollow_Stop(&follower); sample(0,0);
  for(w=0;w<4;++w) assert(!pins[w]&&!drive.requested_cps[w]);
  puts("PASS: upper-half 30-degree edge / 40-degree stopped-heading exit, ambiguous-line rejection, debounce, aligned release and STOP");
}
static void observation_resume_uses_live_line(int side, uint32_t origin, uint8_t at_probe,
                                            uint8_t adjacent)
{
  unsigned i,w,unexpected=0;
  uint8_t inner=side<0?2:4;
  if(adjacent) inner=side<0?3:12; /* middle plus outer, opposite the sign */
  init(1,origin);
  if(at_probe)
  {
    for(i=0;i<3;++i) sample(15,0);
    assert(route.state==SIGN_ROUTE_PROBE && route.direction==0);
  }
  /* Stop on a crossbar, with one inner sensor carrying the straight line
     after a small chassis offset. Neither wheel advances during the pause. */
  for(i=0;i<199;++i)
  {
    observe(side); sample(i<8?15:inner,0);
    for(w=0;w<4;++w) assert(!drive.requested_cps[w]);
    if(i>=4 && route.state!=(at_probe?SIGN_ROUTE_PROBE:SIGN_ROUTE_ARMED)) unexpected=1;
  }
  if(unexpected) fprintf(stderr,"Navigation advanced while observation was stopped: side=%d state=%d\n",side,route.state);
  /* Resumption must correct the visible inner line, not pretend it is white
     just because the pending sign points to the other side. */
  for(i=0;i<12;++i)
  {
    observe(side); sample(inner,0);
    if((adjacent && (drive.requested_cps[0]<0 || drive.requested_cps[2]<0 ||
        drive.requested_cps[0]+drive.requested_cps[2]!=1800 ||
        follower.last_owner!=SIGN_FOLLOW_OWNER_LINE)) ||
       (!adjacent && (drive.requested_cps[0]<=0 || drive.requested_cps[2]<=0)))
    {
      fprintf(stderr,"Pause resume ignores inner line: side=%d mask=%u state=%d targets=%ld/%ld\n",
          side,inner,route.state,(long)drive.requested_cps[0],(long)drive.requested_cps[2]);
      unexpected=1;break;
    }
  }
  assert(unexpected==0 && route.direction==side && route.approach_from_pause);
  /* Live branch evidence after resumption still authorizes the chosen entry. */
  for(i=0;i<3;++i) sample(15,0);
  sample(side<0?8:1,-side*20000);
  assert(route_command.active);
  for(i=0;i<4;++i) sample(6,-side*20000);
  assert(route.state==SIGN_ROUTE_ARC && route.entry_line_ready);
  SignLineFollow_Stop(&follower); sample(0,0);
  for(w=0;w<4;++w) assert(!drive.requested_cps[w]&&!pins[w]);
  puts("PASS: stationary observation cannot select a branch; resume follows the actual inner line; confirmed entry and STOP retained");
}
static void manual_stop_during_observation(void)
{
  unsigned i,w;
  init(1,UINT32_MAX-120U);
  for(i=0;i<10;++i) { observe(1); sample(6,0); }
  assert(follower.observation_paused);
  SignLineFollow_Stop(&follower);
  for(i=0;i<250;++i)
  {
    sample(i<5?0:6,0); /* line transitions and the pause deadline cannot undo STOP */
    assert(!follower.running && !follower.observation_paused);
    for(w=0;w<4;++w) assert(!drive.requested_cps[w]&&!pins[w]);
  }
  puts("PASS: manual STOP during observation remains authoritative after the two-second deadline");
}
static void enter_exit_line(int side, uint32_t origin)
{
  unsigned i,w;
  enter_test_arc(side,origin);
  sample(6,-side*80000);
  for(w=0;w<4;++w) counts[w]+=2000;
  for(i=0;i<4;++i) sample(6,side*65000);
  assert(route.state==SIGN_ROUTE_EXIT_SELECT);
  sample(0,side*5000);
  assert(route.state==SIGN_ROUTE_EXIT_CLEAR && !route_command.active);
}
static void exit_contact_must_end_blind_travel(int side, uint32_t origin)
{
  unsigned i,w,failures=0;
  uint8_t outer=side<0?8:1;
  enter_exit_line(side,origin);
  sample(15,side*5000);
  sample(outer,side*5000);
  if (!(side<0 ? drive.requested_cps[0]==-1800 && drive.requested_cps[2]==1800 :
                 drive.requested_cps[0]==1800 && drive.requested_cps[2]==-1800))
  {
    fprintf(stderr,"Exit crossing tail ignored live edge: side=%d targets=%ld/%ld\n",
        side,(long)drive.requested_cps[0],(long)drive.requested_cps[2]); ++failures;
  }
  enter_exit_line(side,origin);
  sample(9,side*5000);
  sample(0,side*5000);
  if(route_command.active)
  { fprintf(stderr,"Exit straight restarted after real broad contact, side=%d\n",side); ++failures; }

  enter_exit_line(side,origin);
  /* Even without any contact, an aligned exit must use ordinary search,
     rather than command the previous two-second / 250-mm blind straight. */
  sample(0,side*5000);
  if(route_command.active || follower.override_active)
  { fprintf(stderr,"Aligned exit still owns blind straight, side=%d\n",side); ++failures; }
  for(i=0;i<40;++i) sample(0,side*5000);
  if (drive.requested_cps[0]==0 || drive.requested_cps[0]!=-drive.requested_cps[2])
  { fprintf(stderr,"Aligned all-white exit did not return to shared search, side=%d\n",side); ++failures; }
  SignLineFollow_Stop(&follower); sample(0,0);
  for(w=0;w<4;++w) assert(!pins[w]&&!drive.requested_cps[w]);
  assert(failures==0);
  puts("PASS: mode3 alignment ends motor ownership; live exit edge overrides crossing tail; white uses shared search");
}
static void exit_turn_reacquires_before_alignment(int side, uint32_t origin)
{
  unsigned i,w,mask;
  uint8_t outer=side<0?8:1;
  for(mask=0;mask<=15;++mask)
  {
    enter_test_arc(side,origin);
    sample(6,-side*80000);
    for(w=0;w<4;++w) counts[w]+=2000;
    for(i=0;i<4;++i) sample(6,side*65000);
    assert(route.state==SIGN_ROUTE_EXIT_SELECT && route_command.active);
    sample(0,side*55000); /* selected outer is clear */
    sample((uint8_t)mask,side*55000);
    if(mask & outer)
    {
      assert(route.state==SIGN_ROUTE_LOCKED && !route.direction && !route_command.active);
      assert(!follower.override_active); /* same cycle goes to normal tracking */
    }
    else
    {
      assert(route.state!=SIGN_ROUTE_LOCKED && route.direction==side);
      /* Other middle/outer combinations, even a centered pair at zero
         heading, are not the requested success event. */
      for(i=0;i<4;++i) sample((uint8_t)mask,0);
      assert(route.state!=SIGN_ROUTE_LOCKED && route.direction==side);
      sample((uint8_t)(mask|outer),side*55000);
      assert(route.state==SIGN_ROUTE_LOCKED && !route.direction && !route_command.active);
    }
    sample(6,0);
    for(w=0;w<4;++w) assert(drive.requested_cps[w]==LINE_TRACKING_MIDDLE_GUARD_CPS);
    for(i=0;i<10;++i) sample(0,side*90000);
    assert(!route_command.active && !follower.override_active);
    SignLineFollow_Stop(&follower); sample(0,0);
    for(w=0;w<4;++w) assert(!pins[w]&&!drive.requested_cps[w]);
  }
  puts("PASS: selected outer clear/black alone finishes mode3 for every other-sensor combination at 55 degrees; same-cycle tracking and STOP");
}
static void ring_recontact_is_not_exit(int side, uint32_t origin)
{
  unsigned i,w;
  uint8_t outer=side<0?8:1;
  enter_test_arc(side,origin);
  sample(6,-side*80000);
  for(w=0;w<4;++w) counts[w]+=2000;
  for(i=0;i<4;++i) sample(outer,side*35000);
  assert(route.state==SIGN_ROUTE_EXIT_SELECT && route.direction==side);
  for(i=0;i<10;++i)
  {
    sample(outer,side*35000);
    assert(route.state==SIGN_ROUTE_EXIT_SELECT && route.direction==side);
  }
  sample(6,side*35000); /* first clear after exit started */
  assert(route.state==SIGN_ROUTE_EXIT_SELECT);
  sample(15,side*55000); /* all-black is success here: the other three do not veto */
  assert(route.state==SIGN_ROUTE_LOCKED && !route.direction && !route_command.active);
  sample(6,0);
  for(w=0;w<4;++w) assert(drive.requested_cps[w]==LINE_TRACKING_MIDDLE_GUARD_CPS);
  SignLineFollow_Stop(&follower); sample(0,0);
  for(w=0;w<4;++w) assert(!pins[w]&&!drive.requested_cps[w]);
  puts("PASS: outer held black when exit starts cannot finish until clear then black; full-black return succeeds, no angle gate");
}
static void brief_heading_return_must_not_skip_exit(int side, uint32_t origin)
{
  unsigned i,w;
  enter_test_arc(side,origin);
  sample(6,-side*80000);
  for(w=0;w<4;++w) counts[w]+=2000;
  for(w=0;w<4;++w) counts[w]+=12;
  sample(6,side*55000);
  for(w=0;w<4;++w) counts[w]+=12;
  sample(6,side*35000); /* one inward correction, not a proven outgoing straight */
  if(route.state!=SIGN_ROUTE_ARC)
    fprintf(stderr,"brief return skipped turn side=%d state=%d reason=%u owner=%u targets=%ld/%ld\n",
        side,route.state,route.exit_reason,follower.last_owner,
        (long)drive.requested_cps[0],(long)drive.requested_cps[2]);
  assert(route.state==SIGN_ROUTE_ARC && !route_command.active);
  for(i=0;i<4;++i) sample(6,side*55000);
  assert(route.state==SIGN_ROUTE_EXIT_SELECT && route_command.active);
  assert(follower.last_owner==SIGN_FOLLOW_OWNER_ROUTE);
  assert(side<0 ? drive.requested_cps[0]==0 && drive.requested_cps[2]==2200 :
                 drive.requested_cps[0]==2200 && drive.requested_cps[2]==0);
  sample(side<0?8:1,side*55000); /* previously clear outer now black: user exit rule */
  assert(route.state==SIGN_ROUTE_LOCKED && !route.direction && !route_command.active);
  SignLineFollow_Stop(&follower); sample(0,0);
  for(w=0;w<4;++w) assert(!drive.requested_cps[w]&&!pins[w]);
  puts("PASS: brief 55-to-35 heading correction cannot skip exit; 55-degree return dispatches real forward pivot and selected outer finishes");
}
static void direct_observation(int side, uint32_t origin)
{
  unsigned i,w,mask;
  const int32_t stopped_yaw=-side*70000;
  for(mask=1;mask<16;++mask)
  {
    init(1,origin);
    sample(side<0?8:1,0); /* start with a real turning/searching output */
    for(i=0;i<3;++i) sample(15,0);
    for(i=0;i<199;++i)
    {
      observe(side);
      sample((uint8_t)mask,stopped_yaw);
      assert(SignObservation_Paused(tick) && SignObservation_HoldingRoute(tick));
      assert(follower.last_owner==SIGN_FOLLOW_OWNER_OBSERVATION);
      for(w=0;w<4;++w) assert(!drive.requested_cps[w] && !pins[w]);
    }
    /* Exact original 2s deadline: no middle pair prerequisite or timer restart. */
    observe(side); sample((uint8_t)mask,stopped_yaw);
    assert(!SignObservation_HoldingRoute(tick));
    assert(route.direction==side && route.approach_from_pause);
    assert(route.heading_error_mdeg==0 && !route.entry_line_ready);
    assert(follower.last_owner!=SIGN_FOLLOW_OWNER_OBSERVATION);
    assert(follower.last_owner!=SIGN_FOLLOW_OWNER_CENTERING);
    if(mask==6) for(w=0;w<4;++w)
        assert(drive.requested_cps[w]==LINE_TRACKING_MIDDLE_GUARD_CPS);
    SignLineFollow_Stop(&follower); sample(0,stopped_yaw);
    for(w=0;w<4;++w) assert(!drive.requested_cps[w] && !pins[w]);
  }
  /* A manual STOP during a white-line observation survives its deadline. */
  init(1,origin);
  for(i=0;i<10;++i) { observe(side); sample(0,0); }
  assert(SignObservation_SeekingLine() && SignObservation_HoldingRoute(tick));
  SignLineFollow_Stop(&follower);
  for(i=0;i<250;++i)
  {
    sample(i%2?0:6,0);
    for(w=0;w<4;++w) assert(!drive.requested_cps[w]&&!pins[w]);
  }
  assert(!follower.running);
  puts("PASS: nonwhite observation retains fixed 2s; STOP during white search survives subsequent contact and deadline");
}
static void seek_one_middle_before_observation(int side, uint8_t middle, uint32_t origin)
{
  unsigned i,w;
  int32_t stopped_yaw=-side*70000;
  init(1,origin);
  sample(side<0?8:1,0);
  for(i=0;i<3;++i) sample(15,0);
  for(i=0;i<320;++i)
  {
    observe(side); sample(i%4==0?0:(i%4==1?8:(i%4==2?1:9)),stopped_yaw);
    assert(SignObservation_SeekingLine() && !SignObservation_Paused(tick));
    assert(SignObservation_HoldingRoute(tick) && !route_command.active);
    assert(follower.last_owner==SIGN_FOLLOW_OWNER_CENTERING);
    assert(drive.requested_cps[0]!=0 && drive.requested_cps[0]==-drive.requested_cps[2]);
  }
  assert(route.direction==side);
  sample(middle,stopped_yaw); /* a SINGLE middle stops in this very cycle */
  assert(!SignObservation_SeekingLine() && SignObservation_Paused(tick));
  for(w=0;w<4;++w) assert(!drive.requested_cps[w] && !pins[w]);
  for(i=0;i<199;++i)
  {
    observe(side); sample(middle,stopped_yaw);
    assert(SignObservation_HoldingRoute(tick));
    for(w=0;w<4;++w) assert(!drive.requested_cps[w] && !pins[w]);
  }
  observe(side); sample(middle,stopped_yaw);
  assert(!SignObservation_HoldingRoute(tick) && route.direction==side);
  assert(route.approach_from_pause && route.heading_error_mdeg==0 && route.yaw_mdeg==0);
  assert(drive.requested_cps[0]>0 && drive.requested_cps[2]>0);
  for(i=0;i<3;++i) sample(15,stopped_yaw);
  sample(side<0?8:1,stopped_yaw-side*20000);
  for(i=0;i<4;++i) sample(6,stopped_yaw-side*20000);
  assert(route.state==SIGN_ROUTE_ARC && route.entry_line_ready);
  SignLineFollow_Stop(&follower); sample(0,stopped_yaw);
  for(w=0;w<4;++w) assert(!drive.requested_cps[w] && !pins[w]);
  puts("PASS: white observation seeks through outer-only contacts; either single middle stops and starts full 2s, retains sign, rebases yaw and enters ARC");
}
static void biased_stop_exit(int side, int32_t skew, uint8_t pretravel)
{
  unsigned i,w;
  init(1,UINT32_MAX-999U);
  if(pretravel) drive_before_stop(0);
  for(i=0;i<203;++i) { observe(side); sample(6,skew); }
  assert(route.road_reference_valid && route.approach_from_pause);
  assert(route.heading_error_mdeg==0);
  for(i=0;i<3;++i) sample(15,skew);
  sample(side<0?8:1,skew-side*20000);
  for(i=0;i<4;++i) sample(6,skew-side*20000);
  assert(route.state==SIGN_ROUTE_ARC);
  for(i=0;i<30;++i)
  {
    for(w=0;w<4;++w) counts[w]+=16;
    sample(6,skew-side*(20000+(int32_t)i*1800));
  }
  for(i=0;i<50;++i)
  {
    for(w=0;w<4;++w) counts[w]+=16;
    sample(6,skew+side*(-74000+(int32_t)i*2400));
    assert(route.state==SIGN_ROUTE_ARC && !route_command.active);
  }
  for(w=0;w<4;++w) counts[w]+=12;
  sample(6,skew+side*52000);
  for(w=0;w<4;++w) counts[w]+=12;
  sample(6,0);
  assert(route.state==SIGN_ROUTE_ARC && !route_command.active);
  for(i=0;i<30;++i)
  {
    for(w=0;w<4;++w) counts[w]+=12;
    sample(6,0); assert(!route_command.active);
  }
  assert(route.state==SIGN_ROUTE_EXIT_CLEAR && route.exit_reason==2);
  assert(!route_command.active && !follower.override_active);
  assert(!follower.guard.entry_guard_active && !follower.guard.curve_yaw_valid);
  /* Even before completion, opposite current line owns the wheels. */
  sample(side<0?1:8,side*20000);
  assert(side<0 ? drive.requested_cps[0]==1800 && drive.requested_cps[2]==-1800 :
                 drive.requested_cps[0]==-1800 && drive.requested_cps[2]==1800);
  for(i=0;i<30;++i)
  {
    for(w=0;w<4;++w) counts[w]+=12;
    sample(6,0); assert(!route_command.active && !follower.override_active);
  }
  finish_outer(side);  assert(route.state==SIGN_ROUTE_LOCKED && route.direction==0);
  for(i=0;i<4;++i)
  {
    sample(side<0?1:8,side*90000);
    assert(!route_command.active && !follower.override_active);
    assert(side<0 ? drive.requested_cps[0]==1800 && drive.requested_cps[2]==-1800 :
                   drive.requested_cps[0]==-1800 && drive.requested_cps[2]==1800);
  }
  for(i=0;i<40;++i) { sample(0,side*100000); assert(!follower.override_active); }
  assert(drive.requested_cps[0]!=0 && drive.requested_cps[0]==-drive.requested_cps[2]);
  SignLineFollow_Stop(&follower); sample(0,0);
  for(w=0;w<4;++w) assert(!pins[w]&&!drive.requested_cps[w]);
}
int main(void)
{
  int side,skew,pretravel;
  weak_votes_must_not_skip_observation(-1,100);
  weak_votes_must_not_skip_observation(1,UINT32_MAX-120U);
  for(side=-1;side<=1;side+=2) for(skew=-30000;skew<=30000;skew+=10000)
    for(pretravel=0;pretravel<=1;++pretravel) biased_stop_exit(side,skew,(uint8_t)pretravel);
  puts("PASS: actual motor pipeline handles biased stops, passive departure, opposite normal bend, re-loss and STOP with/without preceding straight travel");
  direct_observation(-1,100);
  brief_heading_return_must_not_skip_exit(-1,100);
  brief_heading_return_must_not_skip_exit(1,UINT32_MAX-120U);
  ring_recontact_is_not_exit(-1,100);
  ring_recontact_is_not_exit(1,UINT32_MAX-120U);
  direct_observation(1,UINT32_MAX-120U);
  seek_one_middle_before_observation(-1,2,100);
  seek_one_middle_before_observation(1,4,UINT32_MAX-120U);
  seek_one_middle_before_observation(-1,4,UINT32_MAX-120U);
  seek_one_middle_before_observation(1,2,100);
  exit_turn_reacquires_before_alignment(-1,100);
  exit_turn_reacquires_before_alignment(1,UINT32_MAX-120U);
  exit_contact_must_end_blind_travel(-1,100);
  exit_contact_must_end_blind_travel(1,UINT32_MAX-120U);
  manual_stop_during_observation();
  observation_resume_uses_live_line(1,100,0,0);
  observation_resume_uses_live_line(-1,UINT32_MAX-120U,0,0);
  observation_resume_uses_live_line(1,UINT32_MAX-120U,1,0);
  observation_resume_uses_live_line(-1,100,1,0);
  observation_resume_uses_live_line(1,100,0,1);
  observation_resume_uses_live_line(-1,UINT32_MAX-120U,0,1);
  observation_resume_uses_live_line(1,UINT32_MAX-120U,1,1);
  observation_resume_uses_live_line(-1,100,1,1);
  earlier_exit_timing(-1,100,0); earlier_exit_timing(1,UINT32_MAX-120U,0);
  earlier_exit_timing(-1,UINT32_MAX-120U,1); earlier_exit_timing(1,100,1);
  naturally_departed_before_gate(-1,100,0);
  naturally_departed_before_gate(1,UINT32_MAX-120U,25000);
  departure_evidence_is_bounded(-1); departure_evidence_is_bounded(1);
  paused_heading_reference(-1,100); paused_heading_reference(1,UINT32_MAX-120U);
  continuous_line_exit(-1,100,0); continuous_line_exit(1,UINT32_MAX-120U,0);
  continuous_line_exit(-1,UINT32_MAX-120U,1); continuous_line_exit(1,100,1);
  mode4_drawn_drive(-1); mode4_drawn_drive(1);
  arc_feedback_and_entry_search();
  exit_releases_direction(-1,100); exit_releases_direction(1,100);
  exit_releases_direction(-1,UINT32_MAX-120U); exit_releases_direction(1,UINT32_MAX-120U);
  parity(100); parity(UINT32_MAX-400U);
  slow_profile_matches_key2_settle();
  slow_speed_and_recognition();
  late_choice(-1);late_choice(1);
  ring(-1,100);ring(1,100);ring(-1,UINT32_MAX-120U);ring(1,UINT32_MAX-120U);
  return 0;
}
