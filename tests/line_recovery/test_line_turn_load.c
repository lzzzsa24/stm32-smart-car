/* Real DriveBase + line-load policy. Only physical encoders, battery and PWM
   pins are mocked. These tests validate control decisions, not tyre friction. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "drive_base.h"
#include "line_turn_load.h"
#include "wheel_encoder.h"
#include "battery_monitor.h"
#include "motorPWM.h"
#include "main.h"
#include "line_tracking.h"
#include "motion_advanced.h"
#include "line_wait_guard.h"
#include "line_search_model.h"
#include "buzzer_phrase_40077493715.h"
#include "line_fault_log.h"
#include "diagnostic_uart.h"
#include "line_bypass_turn.h"
#include "line_obstacle_bypass.h"
#include "encoder_linear.h"
#include "encoder_turn.h"

void DiagnosticUart_WriteString(const char *s) { (void)s; }
void DiagnosticUart_WriteUnsigned(uint32_t v) { (void)v; }
void DiagnosticUart_WriteSigned(int32_t v) { (void)v; }

static uint32_t tick;
static int32_t counts[4];
static int16_t pins[4];
static GPIO_PinState buzzer;
static WheelEncoderDiagnostics diagnostics;
uint32_t HAL_GetTick(void) { return tick; }
int HAL_GPIO_ReadPin(GPIO_TypeDef *p,uint16_t n) { (void)p; (void)n; return 1; }
void HAL_GPIO_Init(GPIO_TypeDef *p,GPIO_InitTypeDef *g) { (void)p; (void)g; }
void HAL_GPIO_WritePin(GPIO_TypeDef *p,uint16_t n,GPIO_PinState s)
{ assert(p==Buzzer_GPIO_Port && n==Buzzer_Pin); buzzer=s; }
void WheelEncoder_Start(void) {}
void WheelEncoder_GetCounts(WheelEncoderCounts *c)
{ c->motor1=counts[0]; c->motor2=counts[1]; c->motor3=counts[2]; c->motor4=counts[3]; }
void WheelEncoder_GetDiagnostics(WheelEncoderDiagnostics *d) { *d=diagnostics; }
void BatteryMonitor_Get(BatteryMonitorStatus *b)
{ memset(b,0,sizeof *b); b->valid=1; b->millivolts=7800; }
#define PWM_STUB(n,i) \
  void pwm_motor##n##_forward(int16_t p) { assert(p>=0 && p<=MOTOR_PWM_PERIOD); pins[i]=p; } \
  void pwm_motor##n##_backward(int16_t p) { assert(p>=0 && p<=MOTOR_PWM_PERIOD); pins[i]=(int16_t)-p; }
PWM_STUB(1,0)
PWM_STUB(2,1)
PWM_STUB(3,2)
PWM_STUB(4,3)
static int32_t absolute(int32_t x) { return x<0?-x:x; }
static void reset(void)
{
  memset(counts,0,sizeof counts);
  memset(&diagnostics,0,sizeof diagnostics);
  DriveBase_Init();
  BuzzerPhrase400_Init();
}
static void command(int32_t left,int32_t right,uint8_t assist)
{
  if(assist) DriveBase_PrepareLineTurnAssist(left,right);
  DriveBase_SetSideCps(left,right);
}
static void sample(const int32_t delta[4])
{
  unsigned i;
  for(i=0;i<4;++i) counts[i]+=delta[i];
  tick+=20;
  DriveBase_Task(tick);
}
static void test_recognition_speed_cap(void)
{
  DriveBaseTelemetry t;
  LineTrackingReading reading = {0};
  LineTrackingCommand output = {0};
  unsigned ms, i;
  reset();
  command(4000, 2000, 1);
  DriveBase_SetSpeedLimitCps(1200);
  DriveBase_GetTelemetry(&t);
  assert(t.requested_cps[0] == 1200 && t.requested_cps[2] == 600);
  command(-2700, 2700, 1);
  DriveBase_GetTelemetry(&t);
  assert(t.requested_cps[0] == -1200 && t.requested_cps[2] == 1200);
  DriveBase_SetWheelCps(4000, 2000, -1000, 0);
  DriveBase_GetTelemetry(&t);
  assert(t.requested_cps[0] == 1200 && t.requested_cps[1] == 600 &&
         t.requested_cps[2] == -300 && t.requested_cps[3] == 0);
  DriveBase_SetSpeedLimitCps(0);
  command(4000, 2000, 1);
  DriveBase_GetTelemetry(&t);
  assert(t.requested_cps[0] == 4000 && t.requested_cps[2] == 2000);
  DriveBase_Stop(DRIVE_STOP_COAST);
  DriveBase_SetSpeedLimitCps(1200);
  DriveBase_GetTelemetry(&t);
  assert(t.mode == DRIVE_BASE_STOPPED && t.requested_cps[0] == 0);
  command(0, 0, 0);
  DriveBase_GetTelemetry(&t);
  assert(t.mode == DRIVE_BASE_STOPPED);
  command(2400, 2400, 0);
  {
    const int32_t moving[4] = {24,24,24,24};
    for (i = 0; i < 10; ++i) sample(moving);
  }
  DriveBase_Stop(DRIVE_STOP_BRAKE);
  DriveBase_SetSpeedLimitCps(900);
  command(2400, 2400, 1);
  DriveBase_GetTelemetry(&t);
  assert(t.mode == DRIVE_BASE_BRAKING);
  reset(); /* boot and explicit reset disable the cap */
  command(4000, 4000, 0);
  DriveBase_GetTelemetry(&t);
  assert(t.requested_cps[0] == 4000);
  DriveBase_Stop(DRIVE_STOP_COAST);
  {
    DrivePositionCommand move = {{1000,1000,1000,1000},
        {2500,2500,2500,2500},1000,12,DRIVE_STOP_COAST};
    assert(DriveBase_StartPositionMove(&move));
    DriveBase_SetSpeedLimitCps(1200);
    command(2000,2000,0);
    DriveBase_GetTelemetry(&t);
    assert(t.mode == DRIVE_BASE_POSITION);
    tick += 1001; DriveBase_Task(tick);
    assert(DriveBase_GetFaultMask() & DRIVE_FAULT_TIMEOUT);
    command(2000,2000,0);
    DriveBase_GetTelemetry(&t);
    assert(t.mode == DRIVE_BASE_FAULT);
  }
  /* Real enhanced search owns DriveBase directly (valid=0); it must obey
     the cap too, then return to the original targets after expiry. */
  line_tracking_reset(); reset(); line_tracking_set_no_line_forward(0);
  DriveBase_SetSpeedLimitCps(1200);
  for (ms = 0; ms < 800; ++ms)
  {
    for (i = 0; i < 4; ++i) counts[i] += pins[i]>0 ? 2 : (pins[i]<0 ? -2 : 0);
    ++tick; DriveBase_Task(tick);
    line_tracking_compute(&reading, 3000, &output);
    line_tracking_apply_command(&output, 3599);
    DriveBase_GetTelemetry(&t);
    if (t.mode == DRIVE_BASE_SPEED)
      for (i = 0; i < 4; ++i) assert(absolute(t.requested_cps[i]) <= 1200);
  }
  assert(!output.valid && t.requested_cps[0] == -1200 && t.requested_cps[2] == 1200);
  DriveBase_SetSpeedLimitCps(0);
  ++tick; line_tracking_compute(&reading,3000,&output);
  DriveBase_GetTelemetry(&t);
  assert(t.requested_cps[0] == -LINE_SEARCH_TARGET_CPS);
  line_tracking_reset(); reset();
  puts("PASS: recognition cap reaches real search, preserves ratios, stop/brake/position/fault ownership");
}

static DriveBaseTelemetry trace(int32_t l,int32_t r,unsigned lag,uint8_t assist)
{
  DriveBaseTelemetry t;
  int32_t delta[4]={l/50,l/50,r/50,r/50};
  unsigned k;
  reset();
  delta[lag]=delta[lag]<0?-1:1; /* slow rolling, never a zero-count stall */
  for(k=0;k<30;++k) { command(l,r,assist); sample(delta); }
  DriveBase_GetTelemetry(&t);
  assert(!t.fault_mask && t.mode==DRIVE_BASE_SPEED);
  assert(t.requested_cps[0]==l && t.requested_cps[1]==l);
  assert(t.requested_cps[2]==r && t.requested_cps[3]==r);
  return t;
}
static void compare_load(int32_t l,int32_t r,unsigned lag)
{
  DriveBaseTelemetry baseline=trace(l,r,lag,0), assisted=trace(l,r,lag,1);
  unsigned i;
  for(i=0;i<4;++i)
  {
    if(i==lag)
      assert(absolute(assisted.output_pwm[i])>absolute(baseline.output_pwm[i])+400);
    else assert(assisted.output_pwm[i]==baseline.output_pwm[i]);
  }
  printf("slow wheel %u (%ld,%ld): PWM %d -> %d, same targets\n",
         lag+1,(long)l,(long)r,baseline.output_pwm[lag],assisted.output_pwm[lag]);
}
static DriveBaseTelemetry trace_line_curve(uint8_t assist)
{
  LineTrackingReading reading={1,0,0,0};
  LineTrackingCommand output;
  DriveBaseTelemetry t;
  int32_t delta[4]={1,28,105,105};
  unsigned i;
  line_tracking_reset(); reset();
  line_tracking_set_smooth_mode(0);
  for(i=0;i<30;++i)
  {
    sample(delta);
    line_tracking_compute(&reading,3000,&output);
    assert(output.valid && output.left_cps==1412 && output.right_cps==5273);
    /* Baseline keeps the exact real line targets but withdraws the claim. */
    if(!assist) DriveBase_PrepareLineTurnAssist(0,0);
    DriveBase_SetSideCps(output.left_cps,output.right_cps);
  }
  DriveBase_GetTelemetry(&t);
  assert(!t.fault_mask);
  return t;
}
static DriveBaseTelemetry trace_integrated_cap(unsigned side, uint8_t fixed)
{
  LineTrackingCommand out;
  LineTrackingReading r = {0};
  DriveBaseTelemetry t;
  int32_t delta[4]={1,1,1,1};
  unsigned i;
  r.x1_black=side==0; r.x3_black=side!=0;
  line_tracking_reset(); reset();
  line_tracking_set_no_line_forward(1); line_tracking_set_smooth_mode(1);
  line_tracking_set_turn_gain_percent(200);
  advanced_set_forward_speed_limit(2200);
  for(i=0;i<50;++i)
  {
    line_tracking_compute(&r,3000,&out);
    assert(out.valid && out.left_cps>0 && out.right_cps>0);
    if(fixed) line_tracking_apply_command(&out,2200);
    else advanced_drive_cps(out.left_cps,out.right_cps); /* Deployed KEY1 adapter. */
    sample(delta);
  }
  DriveBase_GetTelemetry(&t);
  assert(!t.fault_mask && t.mode==DRIVE_BASE_SPEED);
  return t;
}
static void test_integrated_line_cap(void)
{
  unsigned side,w;
  DriveBaseTelemetry old, current;
  LineTrackingCommand command={2500,2000,LINE_ACTION_LEFT_ADJUST,1};
  int32_t zero[4]={0};
  for(side=0;side<2;++side)
  {
    old=trace_integrated_cap(side,0); current=trace_integrated_cap(side,1);
    for(w=0;w<4;++w)
    {
      assert(current.requested_cps[w]==old.requested_cps[w]);
      assert(current.requested_cps[w]<=DriveBase_EquivalentCpsFromPwm(2200));
    }
    w=side?0:2;
    printf("KEY1 cap side=%u: target=%ld PWM=%d -> %d\n",side,
           (long)current.requested_cps[w],old.output_pwm[w],current.output_pwm[w]);
    assert(current.output_pwm[w]>old.output_pwm[w]+400);
  }
  line_tracking_apply_command(&command,0); sample(zero);
  DriveBase_GetTelemetry(&current); assert(current.mode==DRIVE_BASE_STOPPED);
  line_tracking_apply_command(&command,3000);
  { int32_t moving[4]={30,30,30,30}; sample(moving); }
  DriveBase_Stop(DRIVE_STOP_BRAKE); line_tracking_apply_command(&command,2200);
  DriveBase_GetTelemetry(&current); assert(current.mode==DRIVE_BASE_BRAKING);
  command.valid=0; line_tracking_apply_command(&command,0);
  DriveBase_GetTelemetry(&current); assert(current.mode==DRIVE_BASE_BRAKING);
  DriveBase_Stop(DRIVE_STOP_COAST); line_tracking_reset();
  line_tracking_set_turn_gain_percent(100);
  puts("PASS: KEY1 final cap retains line assistance; zero cap and braking ownership preserved");
}
static void test_bounded_automatic_waits(void)
{
  LineWaitGuard guard={0};
  LineWaitAction action;
  DriveBaseTelemetry t;
  int32_t zero[4]={0};
  unsigned ms,w,begins=0,active_ticks=0;
  assert(LineWaitGuard_Update(&guard,1,1,0)==LINE_WAIT_NONE);
  assert(LineWaitGuard_Update(&guard,1,1,799)==LINE_WAIT_NONE);
  assert(LineWaitGuard_Update(&guard,1,1,800)==LINE_WAIT_BEGIN_RECOVERY);
  assert(LineWaitGuard_Update(&guard,1,0,1999)==LINE_WAIT_RECOVERING);
  assert(LineWaitGuard_Update(&guard,1,0,2000)==LINE_WAIT_END_RECOVERY);
  assert(LineWaitGuard_Update(&guard,1,1,2799)==LINE_WAIT_NONE);
  assert(LineWaitGuard_Update(&guard,1,1,2800)==LINE_WAIT_BEGIN_RECOVERY);
  assert(LineWaitGuard_Update(&guard,0,1,2801)==LINE_WAIT_NONE);
  assert(LineWaitGuard_Update(&guard,0,1,100000)==LINE_WAIT_NONE);
  assert(LineWaitGuard_Update(&guard,1,1,100001)==LINE_WAIT_NONE);
  LineWaitGuard_Reset(&guard);
  assert(LineWaitGuard_Update(&guard,1,1,UINT32_MAX-400)==LINE_WAIT_NONE);
  assert(LineWaitGuard_Update(&guard,1,1,399)==LINE_WAIT_BEGIN_RECOVERY);
  assert(LineWaitGuard_Update(&guard,1,0,1599)==LINE_WAIT_END_RECOVERY);
  /* Real DriveBase, with a sensor controller repeatedly reissuing stop.
     The supervisor still commands full counter-rotation on every deadline. */
  line_tracking_reset(); reset(); LineWaitGuard_Reset(&guard);
  for(ms=0;ms<10000;++ms)
  {
    for(w=0;w<4;++w) counts[w]+=pins[w]>0?3:(pins[w]<0?-3:0);
    ++tick; DriveBase_Task(tick); DriveBase_GetTelemetry(&t);
    action=LineWaitGuard_Update(&guard,1,
        (uint8_t)(t.mode==DRIVE_BASE_STOPPED || t.mode==DRIVE_BASE_BRAKING || t.fault_mask),tick);
    if(action==LINE_WAIT_BEGIN_RECOVERY)
    { ++begins; DriveBase_Stop(DRIVE_STOP_COAST); DriveBase_ClearFault(); line_tracking_reset(); }
    if(action==LINE_WAIT_BEGIN_RECOVERY || action==LINE_WAIT_RECOVERING)
    {
      LineWaitGuard_Drive(begins%2?1:-1); ++active_ticks;
      DriveBase_GetTelemetry(&t);
      assert(t.mode==DRIVE_BASE_SPEED && !t.fault_mask);
      assert(t.requested_cps[0]==t.requested_cps[1] && t.requested_cps[2]==t.requested_cps[3]);
      assert(t.requested_cps[0]==-t.requested_cps[2] && t.requested_cps[0]!=0);
    }
    else DriveBase_Stop(DRIVE_STOP_BRAKE);
  }
  assert(begins==5 && active_ticks>=5900);
  LineWaitGuard_Reset(&guard); DriveBase_Stop(DRIVE_STOP_COAST);
  for(ms=0;ms<5000;++ms)
  { ++tick; assert(LineWaitGuard_Update(&guard,0,1,tick)==LINE_WAIT_NONE); }
  DriveBase_GetTelemetry(&t); assert(t.mode==DRIVE_BASE_STOPPED && !t.requested_cps[0]);
  /* Real latched drive fault is cleared only at the explicit recovery boundary. */
  line_tracking_reset(); reset(); command(2500,2500,0);
  for(ms=0;ms<100;++ms) sample(zero);
  assert(DriveBase_GetFaultMask()!=0);
  DriveBase_ClearFault(); line_tracking_reset(); LineWaitGuard_Drive(1);
  DriveBase_GetTelemetry(&t);
  assert(!t.fault_mask && t.mode==DRIVE_BASE_SPEED && t.requested_cps[0]==LINE_SEARCH_TARGET_CPS);
  DriveBase_Stop(DRIVE_STOP_COAST); line_tracking_reset();
  puts("PASS: bounded 800-ms waits, 1200-ms recovery, repeated STOP callbacks, latched fault, manual STOP, wrap");
}
static void test_position_coast_handoff(int32_t direction)
{
  DrivePositionCommand move={{-316,-316,-316,-316},{2493,2493,2493,2493},1800,12,DRIVE_STOP_COAST};
  int32_t zero[4]={0}, travel[4]={-150,-20,-20,-20};
  DriveBaseTelemetry t;
  unsigned i;
  for(i=0;i<4;++i) { move.delta_counts[i]*=direction; travel[i]*=direction; }
  reset();
  assert(DriveBase_StartPositionMove(&move));
  sample(zero);
  assert(pins[0]*direction<0 && pins[1]*direction<0);
  sample(travel); /* M1 has entered the <180-count pulse zone. */
  DriveBase_GetTelemetry(&t);
  printf("position pulse handoff: M1 remaining=%ld PWM=%d\n",
         (long)t.position_remaining_counts[0],pins[0]);
  fflush(stdout);
  assert(pins[0]==0); /* No continuous torque during pulse settling. */
  assert(pins[1]*direction<0 && pins[2]*direction<0 && pins[3]*direction<0);
  /* A coasting wheel keeps moving briefly. It must remain unpowered while
     the pulse scheduler waits for stationary counts, not relaunch at once. */
  for(i=0;i<30;++i)
  {
    counts[0]-=direction;
    ++tick; DriveBase_Task(tick);
    assert(!pins[0] && !DriveBase_GetFaultMask());
  }
  for(i=0;i<10;++i) { ++tick; DriveBase_Task(tick); assert(!pins[0]); }
  /* The other wheels catch up; until then the existing progress synchronizer
     correctly holds M1 even though its coast guard has elapsed. */
  counts[1]=counts[2]=counts[3]=counts[0];
  /* Once coast/stability guards elapse, a bounded pulse can actually start. */
  for(i=0;i<30 && pins[0]==0;++i) { ++tick; DriveBase_Task(tick); }
  assert(pins[0]*direction<0 && !DriveBase_GetFaultMask());
  assert(DriveBase_RequestPositionStop(DRIVE_STOP_BRAKE));
  for(i=0;i<200;++i) { ++tick; DriveBase_Task(tick); }
  assert(!DriveBase_GetFaultMask());
  assert(DriveBase_GetPositionState()==DRIVE_POSITION_DONE);
  for(i=0;i<4;++i) assert(pins[i]==0);
  DriveBase_Stop(DRIVE_STOP_COAST);
}

static void test_rolling_loss_reentry(unsigned right)
{
  LineTrackingReading reading;
  LineTrackingCommand output;
  DriveBaseTelemetry t;
  unsigned ms,w,mask,phase_ms,reentries=0;
  int32_t previous_left=0;
  line_tracking_reset(); reset();
  line_tracking_set_no_line_forward(0);
  line_tracking_set_smooth_mode(0);
  /* Six short captures followed by loss, reproducing a narrow stripe at a
     corner. Real DriveBase must keep the outside wheels powered throughout. */
  for(ms=0;ms<1608;++ms)
  {
    for(w=0;w<4;++w) counts[w]+=pins[w]>0?3:(pins[w]<0?-3:0);
    ++tick; DriveBase_Task(tick);
    phase_ms=(ms<300)?0:(ms-300)%218;
    mask=ms<300?5:(phase_ms==0?(right?8:2):(phase_ms>=101 && phase_ms<109?5:0));
    reading=(LineTrackingReading){mask&1,(mask>>1)&1,(mask>>2)&1,(mask>>3)&1};
    line_tracking_compute(&reading,3000,&output);
    line_tracking_apply_command(&output,MOTOR_PWM_PERIOD);
    DriveBase_GetTelemetry(&t);
    assert(!t.fault_mask && t.mode==DRIVE_BASE_SPEED);
    assert(t.requested_cps[0]!=0 && t.requested_cps[2]!=0);
    if(ms>20) assert(right?pins[0]>0 && pins[1]>0:pins[2]>0 && pins[3]>0);
    if(t.requested_cps[0]*t.requested_cps[2]<0)
    {
      assert(t.requested_cps[0]==(right?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS));
      if(previous_left>=0 && !right) ++reentries;
      if(previous_left!=LINE_SEARCH_TARGET_CPS && right) ++reentries;
    }
    previous_left=t.requested_cps[0];
  }
  assert(reentries>=6);
  line_tracking_reset();
  for(w=0;w<4;++w) assert(pins[w]==0);
  assert(!BuzzerPhrase400_IsPlaying());
  printf("PASS: KEY2 narrow-line reentry side=%u has no whole-car stop; operator reset stops all wheels\n",right);
}

static void test_real_search_capture(void)
{
  DriveBaseTelemetry t;
  LineTrackingCommand output={0};
  LineTrackingReading reading={1,0,1,0};
  uint8_t saw_reverse=0, found=0;
  int32_t reverse_origin=0;
  uint32_t found_ms=0;
  unsigned ms,i;
  line_tracking_reset(); reset();
  line_tracking_set_no_line_forward(0);
  line_tracking_set_smooth_mode(0);
  /* Deliberately simple PWM-to-count plant: checks real state-machine
     ownership through loss/retrace/capture, not chassis slip or stopping distance. */
  for(ms=0;ms<2400;++ms)
  {
    for(i=0;i<4;++i) counts[i]+=pins[i]>0?4:(pins[i]<0?-4:0);
    ++tick; DriveBase_Task(tick); DriveBase_GetTelemetry(&t);
    assert(!t.fault_mask);
    assert(t.mode!=DRIVE_BASE_POSITION);
    if(ms>=300 && !found)
    {
      reading.x1_black=reading.x3_black=0;
      if(t.mode==DRIVE_BASE_SPEED && t.requested_cps[0]<0 && t.requested_cps[2]>0)
      {
        if(!saw_reverse) reverse_origin=counts[0];
        saw_reverse=1;
        if(absolute(counts[0]-reverse_origin)>=20)
        { found=1; found_ms=tick; }
      }
    }
    if(found) reading.x1_black=reading.x3_black=1;
    line_tracking_compute(&reading,3000,&output);
    if(output.valid)
    {
      if(!output.left_cps && !output.right_cps) DriveBase_Stop(DRIVE_STOP_COAST);
      else DriveBase_SetSideCps(output.left_cps,output.right_cps);
    }
    if(found && tick-found_ms>750) break;
  }
  assert(saw_reverse && found && tick-found_ms>750);
  assert(output.valid && output.left_cps==5273 && output.right_cps==5273);
  assert(!DriveBase_GetFaultMask());
  line_tracking_reset(); DriveBase_Stop(DRIVE_STOP_COAST);
  assert(!BuzzerPhrase400_IsPlaying() && !buzzer);
  puts("PASS: real line loss -> persistent search -> middle hit -> rolling silent capture -> normal");
}
static void test_real_white_search(void)
{
  LineTrackingReading reading={0};
  LineTrackingCommand output={0};
  DriveBaseTelemetry t;
  unsigned ms,i;
  line_tracking_reset(); reset(); line_tracking_set_no_line_forward(0);
  line_tracking_set_smooth_mode(0);
  for(ms=0;ms<90000;++ms)
  {
    for(i=0;i<4;++i) counts[i]+=pins[i]>0?3:(pins[i]<0?-3:0);
    ++tick; BuzzerPhrase400_Task(tick); DriveBase_Task(tick); DriveBase_GetTelemetry(&t);
    assert(!t.fault_mask && t.mode!=DRIVE_BASE_POSITION);
    line_tracking_compute(&reading,3000,&output);
    if(output.valid)
    {
      if(!output.left_cps && !output.right_cps) DriveBase_Stop(DRIVE_STOP_COAST);
      else DriveBase_SetSideCps(output.left_cps,output.right_cps);
    }
    if(ms>100) assert(!output.valid && t.requested_cps[0]<0 && t.requested_cps[2]>0);
  }
  assert(BuzzerPhrase400_IsPlaying());
  reading.x1_black=reading.x3_black=1;
  for(ms=0;ms<700;++ms)
  {
    for(i=0;i<4;++i) counts[i]+=pins[i]>0?3:(pins[i]<0?-3:0);
    ++tick; BuzzerPhrase400_Task(tick); DriveBase_Task(tick);
    line_tracking_compute(&reading,3000,&output);
    if(output.valid)
    {
      if(!output.left_cps && !output.right_cps) DriveBase_Stop(DRIVE_STOP_COAST);
      else DriveBase_SetSideCps(output.left_cps,output.right_cps);
    }
    assert(!DriveBase_GetFaultMask());
  }
  assert(output.left_cps==5273 && output.right_cps==5273 && !BuzzerPhrase400_IsPlaying() && !buzzer);
  DriveBase_Stop(DRIVE_STOP_COAST); line_tracking_reset();
  puts("PASS: real 90-second rotation/audio -> confirmed line -> silent normal driving");
}
static void test_real_exit_direction_correction(void)
{
  unsigned side,ms,w;
  for(side=0;side<2;++side)
  {
    LineTrackingCommand out={0}; DriveBaseTelemetry t;
    line_tracking_reset(); reset(); line_tracking_set_no_line_forward(0);
    for(ms=0;ms<1000;++ms)
    {
      unsigned mask=ms<100?(side?8:2):0;
      LineTrackingReading r;
      if(ms==300) mask=5;
      if(ms==312) mask=side?2:8;
      for(w=0;w<4;++w) counts[w]+=pins[w]>0?3:(pins[w]<0?-3:0);
      ++tick; DriveBase_Task(tick);
      r=(LineTrackingReading){mask&1,(mask>>1)&1,(mask>>2)&1,(mask>>3)&1};
      line_tracking_compute(&r,3000,&out);
      if(out.valid) DriveBase_SetSideCps(out.left_cps,out.right_cps);
      DriveBase_GetTelemetry(&t);
      assert(!t.fault_mask && t.mode!=DRIVE_BASE_POSITION);
      if(ms>=313)
      {
        assert(t.mode==DRIVE_BASE_SPEED && BuzzerPhrase400_IsPlaying());
        assert((side?-t.requested_cps[0]:t.requested_cps[0])>0);
        assert(t.requested_cps[0]==t.requested_cps[1]);
        assert(t.requested_cps[2]==t.requested_cps[3]);
        assert(t.requested_cps[0]==-t.requested_cps[2]);
      }
      if(ms>=600)
        assert((side?-pins[0]:pins[0])>0 && pins[1]*pins[0]>0 &&
               (side?pins[2]:-pins[2])>0 && pins[3]*pins[2]>0);
    }
    line_tracking_reset();
    assert(!BuzzerPhrase400_IsPlaying());
  }
  puts("PASS: actual DriveBase ramps both corrected exit directions without brake/restart");
}
static void test_real_corner_chatter(void)
{
  unsigned side,ms,w;
  for(side=0;side<2;++side)
  {
    LineTrackingCommand out={0}; DriveBaseTelemetry t;
    line_tracking_reset(); reset(); line_tracking_set_no_line_forward(0);
    for(ms=0;ms<3000;++ms)
    {
      unsigned mask;
      LineTrackingReading r;
      for(w=0;w<4;++w) counts[w]+=pins[w]>0?3:(pins[w]<0?-3:0);
      ++tick; DriveBase_Task(tick);
      if(ms<300 || ms>=2220) mask=5;
      else if(ms>=2100 && ms<2140) mask=side?13:7;
      else if(ms>=2140) mask=0;
      else if(ms<600) mask=side?8:2;
      else if(ms%100<2) mask=5; /* Two-ms isolated flash does not confirm a narrow line. */
      else if((ms/30)%3==0) mask=0;
      else mask=side?12:3;
      r=(LineTrackingReading){mask&1,(mask>>1)&1,(mask>>2)&1,(mask>>3)&1};
      line_tracking_compute(&r,3000,&out);
      if(out.valid) DriveBase_SetSideCps(out.left_cps,out.right_cps);
      DriveBase_GetTelemetry(&t);
      if(ms>=2100 && ms<2200) assert(t.requested_cps[0]>0 && t.requested_cps[2]>0);
      if(ms>=2200 && ms<2250) assert(pins[0]>0 && pins[1]>0 && pins[2]>0 && pins[3]>0);
      if(ms>=600 && ms<2100)
      {
        assert(t.mode==DRIVE_BASE_SPEED && !t.fault_mask);
        assert((side?pins[0]:-pins[0])>0 && (side?-pins[2]:pins[2])>0);
        assert(pins[1]*pins[0]>0 && pins[3]*pins[2]>0);
      }
    }
    assert(out.valid && out.left_cps>0 && out.right_cps>0 && !BuzzerPhrase400_IsPlaying());
    line_tracking_reset(); DriveBase_Stop(DRIVE_STOP_COAST);
    assert(!pins[0] && !pins[1] && !pins[2] && !pins[3]);
  }
  puts("PASS: actual DriveBase keeps four-wheel counter-rotation through corner edge/white chatter");
}
static void test_no_motion_keeps_turn_effort(void)
{
  unsigned wheel, step;
  int direction;
  for(direction=-1;direction<=1;direction+=2) for(wheel=0;wheel<4;++wheel)
  {
    int32_t left=2500*direction, right=-left;
    int32_t delta[4]={left/50,left/50,right/50,right/50};
    int16_t before=0;
    LineFaultRecord record={0};
    reset(); DriveBase_SetLineFaultObservation(1,0,1); delta[wheel]=0;
    for(step=0;step<150;++step)
    {
      command(left,right,1); sample(delta);
      if(step==74) before=pins[wheel]; /* Before the 1600-ms observation. */
    }
    printf("no-motion wheel %u direction %d: before=%d after=%d\n",wheel+1,direction,before,pins[wheel]);
    fflush(stdout);
    assert(absolute(pins[wheel])>=absolute(before));
    assert(!DriveBase_GetFaultMask() && !DriveBase_GetLineDegradedMask());
    assert(LineFaultLog_Get(0,&record) && record.stall_mask==(1U<<wheel));
    assert(!record.direction_mask && !record.signal_mask);
    /* When traction returns, feedback reduces effort; no permanent boost. */
    delta[wheel]=(wheel<2?left:right)/50;
    for(step=0;step<30;++step) { command(left,right,1); sample(delta); }
    assert(absolute(pins[wheel])<absolute(before));
    DriveBase_Stop(DRIVE_STOP_COAST); DriveBase_SetLineFaultObservation(0,0,0);
    assert(!pins[0] && !pins[1] && !pins[2] && !pins[3] && LineFaultLog_Count());
  }
}
static void test_observe_faults(void)
{
  unsigned kind, i;
  DriveBaseTelemetry t;
  LineFaultRecord r={0};
  const int32_t good[4]={50,50,-50,-50};
  for(kind=0;kind<3;++kind)
  {
    int32_t delta[4]={50,50,-50,-50};
    reset(); DriveBase_SetLineFaultObservation(1,5,1);
    command(2500,-2500,1);
    for(i=0;i<10;++i) sample(good);
    if(kind==0) delta[0]=0;
    if(kind==1) delta[0]=-1;
    for(i=0;i<150;++i)
    {
      if(kind==2) diagnostics.illegal_transition_count[0]+=6;
      command(2500,-2500,1); sample(delta);
      assert(!DriveBase_GetFaultMask() && pins[0]>0 && pins[2]<0);
    }
    DriveBase_GetTelemetry(&t);
    assert(t.mode==DRIVE_BASE_SPEED && DriveBase_GetLineDegradedMask()==(kind==0?0:1));
    assert(LineFaultLog_Count()==1 && LineFaultLog_Get(0,&r));
    assert(r.stall_mask==(kind==0?1:0) && r.direction_mask==(kind==1?1:0));
    assert(r.signal_mask==(kind==2?1:0) && r.battery_mv==7800 && r.occurrences>1);
    assert(r.requested[0]==2500 && r.delta[0]==delta[0] && r.sensor_mask==5);
    assert(r.recovery_state==1 && r.degraded_mask==(kind==0?0:1));
    /* No motion retains effort; contradictory/noisy feedback falls back. */
    assert(t.output_pwm[0]>0);
    if(kind==0) assert(t.output_pwm[0]>3400);
    else assert(t.output_pwm[0]<3000);
    DriveBase_Stop(DRIVE_STOP_COAST); DriveBase_SetLineFaultObservation(0,0,0);
    DriveBase_ClearFault();
    assert(!pins[0] && !pins[1] && !pins[2] && !pins[3]);
    assert(LineFaultLog_Count()==1 && !DriveBase_GetLineDegradedMask());
    /* Ordinary speed owner again retains its original stop policy. */
    command(2500,-2500,0);
    delta[0]=0;
    for(i=0;i<100;++i) sample(delta);
    assert(DriveBase_GetFaultMask() & 1);
  }
  /* A position command must not inherit diagnostic-only faults. */
  {
    DrivePositionCommand move={{1000,1000,1000,1000},{2500,2500,2500,2500},100,12,DRIVE_STOP_COAST};
    const int32_t zero[4]={0};
    reset(); DriveBase_SetLineFaultObservation(1,0,1);
    assert(DriveBase_StartPositionMove(&move));
    for(i=0;i<10;++i) sample(zero);
    assert(DriveBase_GetFaultMask() & DRIVE_FAULT_TIMEOUT);
  }
  /* Actual line recovery keeps spinning, sounding and reacquiring with no
     M1 counts; same reset hook cancels movement and keeps the evidence. */
  {
    LineTrackingReading reading={0}; LineTrackingCommand out;
    line_tracking_reset(); reset(); line_tracking_set_no_line_forward(0);
    for(i=0;i<300;++i)
    {
      int32_t delta[4]={0,pins[1]>0?50:(pins[1]<0?-50:0),
          pins[2]>0?50:(pins[2]<0?-50:0),pins[3]>0?50:(pins[3]<0?-50:0)};
      sample(delta); line_tracking_compute(&reading,3000,&out);
      if(out.valid) DriveBase_SetSideCps(out.left_cps,out.right_cps);
    }
    assert(LineFaultLog_Count() && !DriveBase_GetFaultMask() && pins[0]<0 && pins[2]>0);
    assert(BuzzerPhrase400_IsPlaying());
    reading.x1_black=reading.x3_black=1;
    for(i=0;i<60;++i)
    {
      int32_t delta[4]={0,pins[1]>0?50:(pins[1]<0?-50:0),
          pins[2]>0?50:(pins[2]<0?-50:0),pins[3]>0?50:(pins[3]<0?-50:0)};
      sample(delta); line_tracking_compute(&reading,3000,&out);
      if(out.valid) DriveBase_SetSideCps(out.left_cps,out.right_cps);
    }
    assert(!DriveBase_GetFaultMask() && !BuzzerPhrase400_IsPlaying() && pins[0]>0 && pins[2]>0);
    line_tracking_reset(); DriveBase_Stop(DRIVE_STOP_COAST);
    assert(!pins[0] && LineFaultLog_Count());
  }
  puts("PASS: line faults log and continue; bounded fallback; capture/STOP; other owners still stop");
}
static void test_bypass_continuous_turn(int32_t direction, int32_t angle)
{
  DriveBaseTelemetry t;
  int32_t creep[4]={-direction,-direction,direction,direction};
  int32_t zero[4]={0};
  int32_t target=(int32_t)(((int64_t)angle * LINE_SEARCH_EFFECTIVE_TRACK_MM *
      LINE_SEARCH_COUNTS_PER_REV + LINE_SEARCH_CPS_DENOMINATOR/2) /
      LINE_SEARCH_CPS_DENOMINATOR);
  unsigned i,w;
  LineBypassTurn_Stop(); reset();
  assert(LineBypassTurn_Start(direction*angle,1800));
  for(i=0;i<30;++i)
  {
    LineBypassTurn_Task(); sample(creep);
    DriveBase_GetTelemetry(&t);
    assert(t.mode==DRIVE_BASE_SPEED && !t.fault_mask);
    for(w=0;w<4;++w)
    {
      assert(t.requested_cps[w]==(w<2?-direction:direction)*1800);
      /* Slow but legal wheel motion: no staggered coast/pulse tail. */
      assert(pins[w]*(w<2?-direction:direction)>0);
    }
  }
  assert(absolute(pins[0])>2300); /* bounded PI + lag assistance */
  /* Even after one wheel finishes early, all wheels keep a continuous target. */
  counts[0]=-direction*target;
  LineBypassTurn_Task();
  DriveBase_GetTelemetry(&t);
  assert(t.mode==DRIVE_BASE_SPEED && t.requested_cps[0]==-direction*1800);
  counts[0]=counts[1]=-direction*(target+5);
  counts[2]=counts[3]=direction*(target+5);
  LineBypassTurn_Task();
  assert(LineBypassTurn_GetState()==LINE_BYPASS_TURN_RUNNING);
  counts[0]-=direction*5; counts[1]-=direction*5;
  counts[2]+=direction*5; counts[3]+=direction*5;
  for(i=0;i<8;++i) { sample(zero); LineBypassTurn_Task(); }
  assert(LineBypassTurn_GetState()==LINE_BYPASS_TURN_DONE);
  assert(LineBypassTurn_GetAchievedAngleMdeg()*direction>angle);
  for(w=0;w<4;++w) assert(pins[w]==0);
  printf("PASS: continuous bypass angle=%ld direction=%ld, four-wheel effort and settled travel\n",
      (long)angle,(long)direction);
  LineBypassTurn_Stop();
}

static void test_bypass_turn_limits(void)
{
  DriveBaseTelemetry t;
  int32_t zero[4]={0}, creep[4]={-1,-1,1,1};
  unsigned i;
  reset(); EncoderTurn_Init();
  assert(EncoderTurn_Start(15000,0,1800));
  assert(!LineBypassTurn_Start(15000,1800)); /* position owner wins */
  counts[0]=counts[1]=-100; counts[2]=counts[3]=100;
  tick+=20; EncoderTurn_Task(); DriveBase_GetTelemetry(&t);
  assert(t.mode==DRIVE_BASE_POSITION);
  assert(absolute(t.requested_cps[0])<1412);
  printf("Legacy 15-degree bypass: remaining=%ld, reduced target=%ld CPS\n",
      (long)absolute(t.position_remaining_counts[0]),(long)absolute(t.requested_cps[0]));
  EncoderTurn_Stop(); reset();
  assert(!LineBypassTurn_Start(0,1800));
  assert(!LineBypassTurn_Start(15000,0));
  assert(!LineBypassTurn_Start(INT32_MIN,1800));
  assert(LineBypassTurn_Start(15000,1800));
  assert(!LineBypassTurn_Start(-15000,1800));
  DriveBase_Stop(DRIVE_STOP_COAST); LineBypassTurn_Task();
  assert(LineBypassTurn_GetState()==LINE_BYPASS_TURN_FAULT);
  for(i=0;i<4;++i) assert(pins[i]==0);
  LineBypassTurn_Stop(); reset();
  assert(LineBypassTurn_Start(45000,1800));
  for(i=0;i<200 && LineBypassTurn_GetState()==LINE_BYPASS_TURN_RUNNING;++i)
  { LineBypassTurn_Task(); sample(creep); }
  assert(LineBypassTurn_GetState()==LINE_BYPASS_TURN_FAULT);
  assert(LineBypassTurn_GetFaultMask()==0x10); /* too little progress, bounded */
  LineBypassTurn_Stop(); reset();
  assert(LineBypassTurn_Start(15000,1800));
  for(i=0;i<140 && LineBypassTurn_GetState()==LINE_BYPASS_TURN_RUNNING;++i)
  { LineBypassTurn_Task(); sample(zero); }
  assert(LineBypassTurn_GetState()==LINE_BYPASS_TURN_FAULT);
  assert(LineBypassTurn_GetFaultMask()&0x0f); /* real stall remains identifiable */
  LineBypassTurn_Stop(); reset();
  assert(LineBypassTurn_Start(15000,1800));
  counts[0]=0; counts[1]=-1000; counts[2]=counts[3]=1000;
  LineBypassTurn_Task();
  DriveBase_GetTelemetry(&t);
  assert(t.mode==DRIVE_BASE_SPEED); /* three moving wheels cannot hide M1 */
  LineBypassTurn_Stop();
  for(i=0;i<4;++i) assert(pins[i]==0);
  puts("PASS: legacy low-speed tail reproduced; bypass ownership, stall, progress timeout and cancellation");
}

static void test_bypass_state_machine(int8_t direction, uint8_t early_ir)
{
  LineObstacleBypassConfig config;
  LineObstacleBypassInput input={0};
  LineObstacleBypassTelemetry bypass;
  DriveBaseTelemetry drive;
  unsigned i;
  reset(); EncoderLinear_Init(); LineObstacleBypass_GetDefaultConfig(&config);
  config.stop_time_ms=0; config.direction_guard_ms=0;
  LineObstacleBypass_Init(&config);
  input.infrared_valid=1;
  input.left_ir_adc=input.right_ir_adc=3000;
  input.left_ir_threshold=input.right_ir_threshold=1500;
  input.left_ir_hysteresis=input.right_ir_hysteresis=200;
  assert(LineObstacleBypass_Start(direction));
  for(i=0;i<20 && LineObstacleBypass_GetState()!=LINE_BYPASS_TURNING;++i)
  { LineObstacleBypass_Task(&input); tick+=20; }
  assert(LineObstacleBypass_GetState()==LINE_BYPASS_TURNING);
  DriveBase_GetTelemetry(&drive); assert(drive.mode==DRIVE_BASE_SPEED);
  /* IR sees the flank before any encoder travel; default 3-sample filter.
     Previous code classified this valid early completion as fault 0x10. */
  if(early_ir) input.left_ir_adc=input.right_ir_adc=1700;
  for(i=0;i<100 && LineObstacleBypass_GetState()==LINE_BYPASS_TURNING;++i)
  {
    DriveBase_GetTelemetry(&drive);
    if(!early_ir && drive.mode==DRIVE_BASE_SPEED)
    {
      counts[0]+=direction*36; counts[1]+=direction*36;
      counts[2]-=direction*36; counts[3]-=direction*36;
    }
    LineObstacleBypass_Task(&input); tick+=20;
  }
  assert(LineObstacleBypass_GetState()==LINE_BYPASS_DRIVING);
  assert(!LineObstacleBypass_GetFaultMask());
  LineObstacleBypass_GetTelemetry(&bypass);
  if(early_ir) assert(bypass.net_turn_mdeg==0);
  else assert(bypass.net_turn_mdeg * -direction>=45000);
  LineObstacleBypass_Stop();
  DriveBase_GetTelemetry(&drive); assert(drive.mode==DRIVE_BASE_STOPPED);
  for(i=0;i<4;++i) assert(pins[i]==0);
  printf("PASS: real KEY1 bypass direction=%d early_IR=%u transfers turn to translation\n",
      (int)direction,(unsigned)early_ir);
}

int main(void)
{
  LineTurnLoadState s={0};
  DriveBaseTelemetry t, baseline;
  int32_t creep[4]={1,50,-50,-50}, stopped[4]={0}, wrong[4]={-20,50,-50,-50};
  unsigned i;
  /* A single bad sample gets no assistance; the ramp and cap are finite. */
  assert(LineTurnLoad_Update(&s,1,2500,50,20)==0);
  assert(LineTurnLoad_Update(&s,1,2500,50,20)==100);
  for(i=0;i<10;++i) (void)LineTurnLoad_Update(&s,1,2500,50,20);
  assert(s.extra_pwm==600);
  assert(LineTurnLoad_Update(&s,1,2500,2600,20)==0);
  assert(LineTurnLoad_Update(&s,1,2500,-50,20)==0);
  assert(LineTurnLoad_Update(&s,1,350,0,20)==0);
  assert(LineTurnLoad_Update(&s,1,2500,0,100)==0);
  assert(LineTurnLoad_Update(&s,0,2500,0,20)==0);

  compare_load(1412,5273,0); /* normal left curve */
  compare_load(5273,1412,2); /* normal right curve */
  for(i=0;i<4;++i) compare_load(-2500,2500,i);
  for(i=0;i<4;++i) compare_load(2500,-2500,i);
  baseline=trace_line_curve(0); t=trace_line_curve(1);
  assert(t.output_pwm[0]>baseline.output_pwm[0]+400);
  for(i=1;i<4;++i) assert(t.output_pwm[i]==baseline.output_pwm[i]);

  /* Removing the line owner cancels effort even for exactly the same targets. */
  (void)trace(2500,-2500,0,1);
  command(2500,-2500,0); sample(creep);
  DriveBase_GetTelemetry(&t);
  assert(t.output_pwm[0]<3000);
  /* A renewed command can rebuild effort; overspeed removes it immediately. */
  for(i=0;i<10;++i) { command(2500,-2500,1); sample(creep); }
  assert(pins[0]>3300);
  creep[0]=100; command(2500,-2500,1); sample(creep);
  assert(pins[0]<3000);
  creep[0]=1;

  /* An ignored command expires, and a stale preparation cannot be consumed. */
  (void)trace(2500,-2500,0,1);
  for(i=0;i<4;++i) sample(creep);
  assert(pins[0]<3000);
  reset(); DriveBase_PrepareLineTurnAssist(2500,-2500); tick+=21;
  DriveBase_SetSideCps(2500,-2500);
  for(i=0;i<15;++i) sample(creep);
  assert(pins[0]<3000);

  /* Capped/mismatched commands cannot accidentally claim the prepared power. */
  reset();
  for(i=0;i<30;++i)
  {
    DriveBase_PrepareLineTurnAssist(3000,-3000);
    DriveBase_SetSideCps(2500,-2500); sample(creep);
  }
  assert(pins[0]<3000);
  /* Equal targets never opt in (straight/crossing/straight capture). */
  baseline=trace(2500,2500,0,0); t=trace(2500,2500,0,1);
  for(i=0;i<4;++i) assert(t.output_pwm[i]==baseline.output_pwm[i]);

  /* Brake, position and fault ownership survive an attempted line command. */
  (void)trace(2500,-2500,0,1);
  DriveBase_Stop(DRIVE_STOP_BRAKE);
  command(2500,-2500,1); DriveBase_GetTelemetry(&t);
  assert(t.mode==DRIVE_BASE_BRAKING);
  DriveBase_Stop(DRIVE_STOP_COAST);
  for(i=0;i<4;++i) assert(pins[i]==0);
  {
    DrivePositionCommand move={{1000,1000,1000,1000},{2500,2500,2500,2500},1000,12,DRIVE_STOP_COAST};
    assert(DriveBase_StartPositionMove(&move));
    command(2500,-2500,1); DriveBase_GetTelemetry(&t);
    assert(t.mode==DRIVE_BASE_POSITION && t.position_state==DRIVE_POSITION_RUNNING);
    tick+=1001; DriveBase_Task(tick);
    assert(DriveBase_GetFaultMask() & DRIVE_FAULT_TIMEOUT);
  }
  /* No encoder response still latches stall; no automatic clearing/retry. */
  reset();
  for(i=0;i<100;++i) { command(2500,-2500,1); sample(stopped); }
  assert((DriveBase_GetFaultMask() & 0x0fU)!=0);
  tick+=100; DriveBase_Task(tick);
  command(2500,-2500,1);
  for(i=0;i<4;++i) assert(pins[i]==0);
  /* Real reverse/sign and illegal-transition guards still latch their bits. */
  reset();
  for(i=0;i<20;++i) { command(2500,-2500,1); sample(wrong); }
  assert(DriveBase_GetFaultMask() & DRIVE_FAULT_DIRECTION);
  reset();
  for(i=0;i<5;++i)
  {
    diagnostics.illegal_transition_count[1]+=6;
    command(2500,-2500,1); sample(creep);
  }
  assert(DriveBase_GetFaultMask() & DRIVE_FAULT_ENCODER_SIGNAL);

  tick=UINT32_MAX-200;
  test_recognition_speed_cap();
  test_rolling_loss_reentry(0);
  test_rolling_loss_reentry(1);
  test_position_coast_handoff(1);
  test_position_coast_handoff(-1);
  test_real_search_capture();
  test_real_white_search();
  test_real_corner_chatter();
  test_integrated_line_cap();
  test_bounded_automatic_waits();
  test_real_exit_direction_correction();
  test_no_motion_keeps_turn_effort();
  test_observe_faults();
  test_bypass_turn_limits();
  test_bypass_continuous_turn(1,15000);
  test_bypass_continuous_turn(-1,45000);
  tick=UINT32_MAX-200;
  test_bypass_continuous_turn(-1,15000);
  test_bypass_state_machine(1,1);
  test_bypass_state_machine(-1,1);
  test_bypass_state_machine(1,0);
  test_bypass_state_machine(-1,0);
  (void)trace(2500,-2500,0,1);
  for(i=0;i<4;++i) sample(creep);
  assert(pins[0]<3000);
  puts("PASS: real speed loop, independent lagging wheels, ownership/expiry, overspeed, rails, faults, wrap");
  return 0;
}
