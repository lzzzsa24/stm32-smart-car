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
  paused=SignObservation_Paused(tick);
  SignRoute_UpdateObservationPause(paused,tick);
  SignRoute_Step(mask,tick,&route_command); SignRoute_GetStatus(tick,&route);
  action=SignLineFollow_Step(&follower,&reading,3000,&route,&route_command,paused);
  DriveBase_Task(tick); DriveBase_GetTelemetry(&drive);
  return action;
}
static void observe(int side)
{
  VisionDetection d={0}; d.class_id=side<0?0:1; d.score=80;
  d.center_x=160; d.center_y=120; d.sequence=++seq; d.received_ms=tick;
  SignRoute_GetStatus(tick,&route);
  SignObservation_AllowPause(route.direction==0 && (route.state==SIGN_ROUTE_IDLE ||
      route.state==SIGN_ROUTE_ARMED || route.state==SIGN_ROUTE_PROBE || route.state==SIGN_ROUTE_WAIT_SIGN));
  SignObservation_ObserveDetection(&d,tick);
  SignRoute_ObserveDetection(&d);
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
  for(pass=0;pass<2;++pass) /* shared slow baseline, then the sole KEY3 sign binding */
  {
    init(pass!=0,origin);
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
        a=line_tracking_compute_slow(&reading,3000,&normal);
        /* Shared slow profile; recovery may supply its own straight command. */
        if(normal.valid && (a==LINE_ACTION_FORWARD || a==LINE_ACTION_CROSSING))
          normal.left_cps=normal.right_cps=DriveBase_EquivalentCpsFromPwm(2200);
        line_tracking_apply_command(&normal,3599);
        action=normalized(a);
      }
      else
      {
        reading=line_tracking_read();
        SignRoute_UpdateYaw(0,1);
        SignRoute_UpdateEncoders(counts[0],counts[1],counts[2],counts[3]);
        SignRoute_Step(mask,tick,&route_command); SignRoute_GetStatus(tick,&route);
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
  puts("PASS: 2000 sign samples match shared slow tracking/search, all wheel outputs and actions");
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
    /* The ordinary KEY2 API in its actual rejoin/settle state is the oracle,
       not the new slow-profile entry. The same first contact precedes each mask. */
    init(0,100); line_tracking_rejoin_from_bypass(6);
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
    for(w=0;w<4;++w) assert(drive.requested_cps[w]==1412);
  }
  /* A sign-mode run must not force KEY2 to stay slow after a mode switch. */
  SignLineFollow_Stop(&follower); line_tracking_start_following();
  for(i=0;i<100;++i) key2_step(6);
  for(w=0;w<4;++w)
    assert(drive.requested_cps[w]==DriveBase_EquivalentCpsFromPwm(2700));
  puts("PASS: all 16 masks match actual KEY2 settle targets, 30s steady slow travel, KEY2 cruise restored on switch");
}
static void slow_speed_and_recognition(void)
{
  unsigned i;
  uint32_t pause_start;
  const int32_t cruise=DriveBase_EquivalentCpsFromPwm(2200);
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
  assert(side<0?drive.requested_cps[0]<0:drive.requested_cps[2]<0);
  for(angle=21;angle<=80;++angle)
  {
    for(w=0;w<4;++w)counts[w]+=22;
    tick+=40; sample(selected,-side*angle*1000);
    assert(route.state==SIGN_ROUTE_ARC && !route_command.active);
    assert(drive.requested_cps[0]>=0&&drive.requested_cps[2]>=0);
    assert(drive.requested_cps[0]+drive.requested_cps[2]==2200);
  }
  for(angle=1;angle<=171;++angle)
  {
    for(w=0;w<4;++w)counts[w]+=22;
    tick+=40; sample(6,side*(angle-80)*1000);
  }
  assert(route.state==SIGN_ROUTE_EXIT_SELECT && route_command.active); /* qualified exit owns heading until aligned */
  for(angle=90;angle>=12;--angle)
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
    assert(drive.requested_cps[0]>0&&drive.requested_cps[0]==drive.requested_cps[2]);
    assert(drive.requested_cps[0]==DriveBase_EquivalentCpsFromPwm(2200));
  }
  for(i=0;i<2;++i){tick+=40;sample(6,-side*12000);}
  assert(route.state==SIGN_ROUTE_LOCKED);
  sample(6,-side*12000); assert(drive.requested_cps[0]>1200);
  SignLineFollow_Stop(&follower); sample(selected,0);
  for(w=0;w<4;++w)assert(!pins[w]&&!drive.requested_cps[w]);
  puts("PASS: real route -> KEY2 follower -> DriveBase, selected branch, forward arc, gyro exit, rejoin and STOP");
}
static void late_choice(int side)
{
  unsigned i;
  init(1,100);
  for(i=0;i<3;++i)sample(15,0);
  for(i=0;i<40;++i)sample(0,0);
  assert(route.state==SIGN_ROUTE_WAIT_SIGN);
  for(i=0;i<203;++i){observe(side);sample(0,0);}
  assert(route.state==SIGN_ROUTE_SELECTING);
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
  assert(side<0 ? drive.requested_cps[0]==0 && drive.requested_cps[2]>0 :
                  drive.requested_cps[2]==0 && drive.requested_cps[0]>0);
  sample(6,-side*SIGN_GYRO_TANGENT_ENTRY_MDEG);
  assert(route.state==SIGN_ROUTE_SELECTING && route_command.active);
  assert(drive.requested_cps[0]==drive.requested_cps[2] && drive.requested_cps[0]>0);
  for(w=0;w<4;++w) counts[w]+=400;
  sample(0,-side*SIGN_GYRO_TANGENT_ENTRY_MDEG);
  for(i=0;i<4;++i) sample(side<0?8:1,-side*SIGN_GYRO_TANGENT_ENTRY_MDEG);
  assert(route.state==SIGN_ROUTE_ARC);
  sample(0,-side*SIGN_GYRO_TANGENT_ENTRY_MDEG+side*10000L);
  assert(route_command.gentle_arc);
  assert(drive.requested_cps[0]==(side<0?outer:inner));
  assert(drive.requested_cps[2]==(side<0?inner:outer));
  assert(drive.requested_cps[0]>0 && drive.requested_cps[2]>0);
  SignLineFollow_Stop(&follower); sample(0,0);
  for(w=0;w<4;++w) assert(!pins[w]&&!drive.requested_cps[w]);
}
static void enter_test_arc(int side, uint32_t origin)
{
  unsigned i;
  init(1,origin);
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
    if (!(side<0 ? drive.requested_cps[0]==2200 && drive.requested_cps[2]==0 :
                  drive.requested_cps[0]==0 && drive.requested_cps[2]==2200))
    {
      fprintf(stderr,"ARC bar->outer lost feedback side=%d targets=%ld/%ld\n",
          side,(long)drive.requested_cps[0],(long)drive.requested_cps[2]); ++failures;
    }
    assert(route.state==SIGN_ROUTE_ARC && !route_command.active);

    /* Both middle sensors cannot describe curve direction. Fresh yaw shows
       the actual curvature has reversed after the entry apex. */
    enter_test_arc(side,UINT32_MAX-120U);
    for(angle=1;angle<=6;++angle) sample(6,side*(angle-20)*1000);
    sample(0,-side*14000);
    if (!(side<0 ? drive.requested_cps[0]>0 : drive.requested_cps[2]>0))
    {
      fprintf(stderr,"ARC loss reused entry direction side=%d targets=%ld/%ld\n",
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
  puts("PASS: ARC raw-line priority, measured curve search direction, entry sweep beyond 25 degrees and STOP");
}
static void exit_releases_direction(int side, uint32_t origin)
{
  unsigned i,w;
  uint8_t opposite=side<0?1:8;
  enter_test_arc(side,origin);
  sample(6,-side*80000); /* entry apex */
  for(w=0;w<4;++w) counts[w]+=2000;
  sample(6,side*80000); /* 160-degree arc, before the 170-degree trigger */
  for(i=0;i<4;++i) sample(6,0); /* already following the aligned outgoing line */
  assert(route.state==SIGN_ROUTE_LOCKED && route.direction==0);
  assert(!route_command.active && route_command.just_finished);
  assert(!follower.override_active && !follower.guard.entry_guard_active);
  assert(!follower.guard.route_hint && !follower.guard.curve_yaw_valid);
  for(w=0;w<4;++w) assert(drive.requested_cps[w]==1412);

  /* A new bend on the ordinary line points opposite to the old sign. Its
     current sensor contact, not the completed route, must choose the wheels. */
  for(i=0;i<6;++i)
  {
    sample(opposite,0);
    assert(!route_command.active && route.direction==0);
    assert(side<0 ? drive.requested_cps[0]==2200 && drive.requested_cps[2]==0 :
                   drive.requested_cps[0]==0 && drive.requested_cps[2]==2200);
  }
  for(i=0;i<40;++i) sample(0,side*100000);
  assert(route.state==SIGN_ROUTE_LOCKED && route.direction==0 && !route_command.active);
  assert(!follower.override_active); /* old ARC's +/-25-degree sector cannot intervene */
  assert(side<0 ? drive.requested_cps[0]==LINE_SEARCH_TARGET_CPS :
                 drive.requested_cps[0]==-LINE_SEARCH_TARGET_CPS);
  assert(drive.requested_cps[0]==-drive.requested_cps[2]);
  for(i=0;i<10;++i) sample(6,0);
  for(w=0;w<4;++w) assert(drive.requested_cps[w]==1412);

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
  assert(route.state==SIGN_ROUTE_LOCKED && route.direction==0 && !route_command.active);
  for(w=0;w<4;++w) assert(drive.requested_cps[w]==1412);
  SignLineFollow_Stop(&follower); sample(0,side*100000);
  for(w=0;w<4;++w) assert(!pins[w]&&!drive.requested_cps[w]);
  puts("PASS: exit clears L/R and route motor ownership; opposite bend/re-loss use shared follower; no delayed exit turn");
}
static void continuous_line_exit(int side, uint32_t origin, uint8_t early_edge)
{
  unsigned i,w;
  int angle, start_heading=early_edge?70000:90000;
  uint8_t selected=side<0?8:1;
  enter_test_arc(side,origin);
  sample(6,-side*80000);
  for(w=0;w<4;++w) counts[w]+=2000;
  /* An outside contact before the exit region is still ordinary arc tracking. */
  for(i=0;i<4;++i) sample(selected,side*60000);
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
  sample(15,side*start_heading); assert(!route_command.active);
  sample(9,side*start_heading); assert(!route_command.active);
  for(angle=start_heading-5000;angle>10000;angle-=5000)
  {
    sample(6,side*angle);
    assert(route_command.active && route.state==SIGN_ROUTE_EXIT_SELECT);
  }
  for(i=0;i<5;++i) sample(6,side*10000);
  assert(route.state==SIGN_ROUTE_LOCKED && route.direction==0 && !route_command.active);
  for(w=0;w<4;++w) assert(drive.requested_cps[w]==1412);
  for(i=0;i<40;++i) sample(0,side*120000);
  assert(route.state==SIGN_ROUTE_LOCKED && !route_command.active && !follower.override_active);
  SignLineFollow_Stop(&follower); sample(0,0);
  for(w=0;w<4;++w) assert(!pins[w]&&!drive.requested_cps[w]);
  puts("PASS: continuous ring line cannot cancel the exit choice; aligned outgoing line releases it; no renewed lap command");
}
static void paused_heading_reference(int side, uint32_t origin)
{
  unsigned i,w;
  const int32_t stopped_yaw=side*10000;
  init(1,origin);
  for(i=0;i<203;++i) { observe(side); sample(6,stopped_yaw); }
  assert(route.approach_from_pause && route.heading_error_mdeg==0);
  /* The approach can need a small correction between the observation stop
     and the crossbar. That must not silently redefine the outgoing heading. */
  for(i=0;i<3;++i) sample(15,stopped_yaw+side*20000);
  assert(route.state==SIGN_ROUTE_PROBE && route.heading_error_mdeg==side*20000);
  assert(route.approach_from_pause);
  sample(side<0?8:1,stopped_yaw-side*20000);
  for(i=0;i<4;++i) sample(6,stopped_yaw-side*20000);
  assert(route.state==SIGN_ROUTE_ARC);
  sample(6,stopped_yaw-side*80000);
  for(w=0;w<4;++w) counts[w]+=2000;
  for(i=0;i<4;++i) sample(6,stopped_yaw);
  assert(route.state==SIGN_ROUTE_ARC && !route_command.active); /* midpoint also faces zero */
  for(i=0;i<4;++i) sample(6,stopped_yaw+side*80000);
  assert(route.state==SIGN_ROUTE_ARC);
  for(i=0;i<4;++i) sample(6,stopped_yaw);
  assert(route.state==SIGN_ROUTE_LOCKED && route.direction==0 && route.heading_error_mdeg==0);
  for(w=0;w<4;++w) assert(drive.requested_cps[w]==1412);
  puts("PASS: 2-second stop anchors outgoing heading despite later approach correction; midpoint zero does not end the arc");
}
int main(void)
{
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
