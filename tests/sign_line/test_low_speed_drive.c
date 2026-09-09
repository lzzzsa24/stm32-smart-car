/* Real DriveBase -> mocked PWM pins -> synthetic inertial wheel response.
   Checks the actuator rail, not just requested targets. Not a floor model. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "drive_base.h"
#include "wheel_encoder.h"
#include "battery_monitor.h"
#include "motorPWM.h"
#include "line_fault_log.h"
static uint32_t tick;
static int32_t counts[4], velocity[4], fraction[4];
static int16_t pins[4];
static uint8_t reverse_feedback;
static uint8_t loaded;
uint32_t HAL_GetTick(void) { return tick; }
void WheelEncoder_Start(void) {}
void WheelEncoder_GetCounts(WheelEncoderCounts *c)
{ c->motor1=counts[0]; c->motor2=counts[1]; c->motor3=counts[2]; c->motor4=counts[3]; }
void WheelEncoder_GetDiagnostics(WheelEncoderDiagnostics *d) { memset(d,0,sizeof(*d)); }
void BatteryMonitor_Get(BatteryMonitorStatus *b)
{ memset(b,0,sizeof(*b)); b->valid=1; b->millivolts=7800; }
void LineFaultLog_Init(void) {}
void LineFaultLog_Record(const LineFaultRecord *r) { (void)r; }
#define PWM_STUB(n,i) \
 void pwm_motor##n##_forward(int16_t p) { assert(p>=0 && p<=3599); pins[i]=p; } \
 void pwm_motor##n##_backward(int16_t p) { assert(p>=0 && p<=3599); pins[i]=(int16_t)-p; }
PWM_STUB(1,0) PWM_STUB(2,1) PWM_STUB(3,2) PWM_STUB(4,3)

static void sample(void)
{
  unsigned m;
  for(m=0;m<4;++m)
  {
    int32_t driven=pins[m]>0?2000:(pins[m]<0?-2000:0);
    if (loaded)
    {
      /* Synthetic static friction: turning needs more breakaway power than
         straight travel; smaller holding power is enough once moving. */
      int32_t power=pins[m]<0?-(int32_t)pins[m]:pins[m];
      int32_t speed=velocity[m]<0?-velocity[m]:velocity[m];
      int32_t start_power=loaded==2?3100:2800;
      if (power<2500 || (speed<100 && power<start_power)) driven=0;
    }
    velocity[m]=(velocity[m]*3+driven)/4;
    fraction[m]+=velocity[m]*20;
    counts[m]+=(reverse_feedback && m==0 ? -1 : 1)*(fraction[m]/1000);
    fraction[m]%=1000;
  }
  tick+=20; DriveBase_Task(tick);
}

static void run(uint8_t enabled, int l, int r)
{
  unsigned i,m,off[4]={0}; int32_t start[4];
  DriveBaseTelemetry d;
  memset(counts,0,sizeof(counts)); memset(velocity,0,sizeof(velocity));
  memset(fraction,0,sizeof(fraction)); memset(pins,0,sizeof(pins)); tick=l<0?UINT32_MAX-100U:0U;
  DriveBase_Init();
  DriveBase_SetSignLowSpeedMode(enabled);
  DriveBase_SetLineFaultObservation(1,6,1);
  DriveBase_SetSideCps(l,r);
  for(i=0;i<200;++i) sample();
  memcpy(start,counts,sizeof(start));
  for(i=0;i<100;++i)
  {
    sample();
    for(m=0;m<4;++m) if(!pins[m]) ++off[m];
  }
  DriveBase_GetTelemetry(&d);
  printf("low=%u load=%u requested=%d/%d measured-average=%ld/%ld PWM-off=%u/%u\n",enabled,loaded,l,r,
      (long)(counts[0]-start[0])/2,(long)(counts[2]-start[2])/2,off[0],off[2]);
  fflush(stdout);
  if(enabled)
  {
    for(m=0;m<4;++m)
    {
      int target=m<2?l:r;
      int32_t mean=(counts[m]-start[m])/2;
      assert(mean>=target-80 && mean<=target+80);
      assert(off[m]>0); /* true coast intervals, not identical continuous floor */
    }
  }
  else assert(pins[0]==2200 && pins[2]==2200); /* reproduce old collapsed steering */
  if(enabled && !loaded)
  {
    DriveBase_SetSignLowSpeedMode(0);
    DriveBase_SetSideCps(400,1200);
    for(i=0;i<100;++i) sample();
    assert(pins[0]==2200 && pins[2]==2200); /* mode exit restores original rail */
  }
  DriveBase_Stop(DRIVE_STOP_COAST);
  for(i=0;i<5;++i) sample();
  for(m=0;m<4;++m) assert(pins[m]==0);
  assert(d.fault_mask==0);
}

static void loaded_start(void)
{
  unsigned i,m;
  /* Real lower-output baseline cannot break away in this model. */
  memset(counts,0,sizeof(counts)); memset(velocity,0,sizeof(velocity));
  memset(fraction,0,sizeof(fraction)); memset(pins,0,sizeof(pins)); tick=0;
  loaded=2;
  DriveBase_Init(); DriveBase_SetSignLowSpeedMode(1);
  DriveBase_SetLineFaultObservation(1,6,1);
  DriveBase_SetSideCps(-500,500);
  for(i=0;i<50;++i) sample();
  for(m=0;m<4;++m)
    assert((m<2?-counts[m]:counts[m])>100); /* all wheels start within 1 second */
  DriveBase_Stop(DRIVE_STOP_COAST);
  for(m=0;m<4;++m) assert(pins[m]==0);

  loaded=1; run(1,500,500);
  loaded=2; run(1,400,1200); run(1,1200,400);
  run(1,-500,500); run(1,500,-500); run(1,1147,0);
  loaded=0;
}

static void position_unchanged(void)
{
  int16_t baseline[80][4]; unsigned pass,i,m;
  DrivePositionCommand p={{300,300,300,300},{500,500,500,500},5000,8,DRIVE_STOP_COAST};
  for(pass=0;pass<2;++pass)
  {
    memset(counts,0,sizeof(counts)); memset(velocity,0,sizeof(velocity));
    memset(fraction,0,sizeof(fraction)); memset(pins,0,sizeof(pins)); tick=0;
    DriveBase_Init(); DriveBase_SetSignLowSpeedMode((uint8_t)pass);
    assert(DriveBase_StartPositionMove(&p));
    for(i=0;i<80;++i)
    {
      sample();
      for(m=0;m<4;++m)
        if(!pass) baseline[i][m]=pins[m]; else assert(baseline[i][m]==pins[m]);
    }
  }
}

static void degraded_feedback(void)
{
  unsigned i,off=0;
  memset(counts,0,sizeof(counts)); memset(velocity,0,sizeof(velocity));
  memset(fraction,0,sizeof(fraction)); memset(pins,0,sizeof(pins)); tick=0;
  DriveBase_Init(); DriveBase_SetSignLowSpeedMode(1);
  DriveBase_SetLineFaultObservation(1,6,1);
  DriveBase_SetSideCps(500,500); reverse_feedback=1;
  for(i=0;i<200;++i) sample();
  assert(DriveBase_GetLineDegradedMask() & 1U);
  for(i=0;i<100;++i)
  {
    sample(); assert(pins[0]>=0);
    if(!pins[0]) ++off;
  }
  assert(off>0 && off<100); /* bad encoder does not force continuous drive or reverse */
  DriveBase_Stop(DRIVE_STOP_COAST); reverse_feedback=0;
}
int main(void)
{
  run(0,400,1200); run(1,400,1200); run(1,500,500);
  run(1,-500,500); run(1,1147,0); run(0,400,1200);
  position_unchanged();
  degraded_feedback();
  loaded_start();
  puts("PASS: real PWM low-speed feedback, mirrored wheel signs, zero side, STOP and opt-out");
  return 0;
}
