/* Production FIFO service, gyro turn, bypass FSM and DriveBase together.
   Only peripherals are mocked. The simple PWM/encoder plant and independent
   yaw gain are test inputs, NOT a measured tyre/surface/braking model. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "mpu6050_yaw.h"
#include "gyro_turn.h"
#include "drive_base.h"
#include "line_obstacle_bypass.h"
#include "line_bypass_travel.h"
#include "line_bypass_turn.h"
#include "line_wait_guard.h"
#include "wheel_encoder.h"
#include "battery_monitor.h"
#include "motorPWM.h"

static uint32_t tick;
static int32_t counts[4], velocity[4], fraction[4];
static int16_t pins[4];
static uint8_t registers[256], fifo[1024], no_yaw, stall_wheel;
static uint16_t fifo_size;
static int32_t yaw_gain;
static unsigned saw_brake, saw_counter, saw_forward;
uint32_t HAL_GetTick(void) { return tick; }
void DiagnosticUart_WriteString(const char *s) { (void)s; }
void DiagnosticUart_WriteUnsigned(uint32_t v) { (void)v; }
void DiagnosticUart_WriteSigned(int32_t v) { (void)v; }
void WheelEncoder_Start(void) {}
void WheelEncoder_GetCounts(WheelEncoderCounts *c)
{ c->motor1=counts[0]; c->motor2=counts[1]; c->motor3=counts[2]; c->motor4=counts[3]; }
void WheelEncoder_GetDiagnostics(WheelEncoderDiagnostics *d) { memset(d,0,sizeof *d); }
void BatteryMonitor_Get(BatteryMonitorStatus *b)
{ memset(b,0,sizeof *b); b->valid=1; b->millivolts=7800; }
#define PWM_STUB(n,i) \
 void pwm_motor##n##_forward(int16_t p) { assert(p>=0 && p<=MOTOR_PWM_PERIOD); pins[i]=p; } \
 void pwm_motor##n##_backward(int16_t p) { assert(p>=0 && p<=MOTOR_PWM_PERIOD); pins[i]=(int16_t)-p; }
PWM_STUB(1,0)
PWM_STUB(2,1)
PWM_STUB(3,2)
PWM_STUB(4,3)
uint8_t MpuBus_Init(void) { return 1; }
uint8_t MpuBus_Write(uint8_t reg,uint8_t v)
{ registers[reg]=v; if(reg==0x6a && v==4) fifo_size=0; return 1; }
uint8_t MpuBus_Read(uint8_t reg,uint8_t *p,uint16_t n)
{
  if(reg==0x3a) { *p=0; return 1; }
  if(reg==0x72) { p[0]=(uint8_t)(fifo_size>>8); p[1]=(uint8_t)fifo_size; return 1; }
  if(reg==0x74)
  {
    assert(n<=96 && n<=fifo_size); memcpy(p,fifo,n);
    fifo_size=(uint16_t)(fifo_size-n); memmove(fifo,fifo+n,fifo_size); return 1;
  }
  assert(n==1); *p=registers[reg]; return 1;
}
static void packet(int32_t z)
{
  uint8_t *p=fifo+fifo_size;
  assert(fifo_size+12<=sizeof fifo); memset(p,0,12); p[4]=0x40;
  p[10]=(uint8_t)((uint16_t)z>>8); p[11]=(uint8_t)z;
  fifo_size+=12;
}
static DriveBaseTelemetry drive(void)
{ DriveBaseTelemetry d; DriveBase_GetTelemetry(&d); return d; }
static void plant(uint8_t service_imu)
{
  unsigned i;
  int32_t rate;
  DriveBaseTelemetry d=drive();
  if(d.mode==DRIVE_BASE_BRAKING) ++saw_brake;
  if(pins[0]*pins[2]<0) ++saw_counter;
  if(pins[0]>0 && pins[2]>0) ++saw_forward;
  for(i=0;i<4;++i)
  {
    velocity[i]+=(pins[i]*2-velocity[i])/3;
    if(stall_wheel==i+1) velocity[i]=0;
    fraction[i]+=velocity[i]*10; counts[i]+=fraction[i]/1000; fraction[i]%=1000;
  }
  /* Changing this gain changes chassis yaw without changing wheel counts. */
  rate=no_yaw ? 0 : (velocity[2]+velocity[3]-velocity[0]-velocity[1])*yaw_gain/2;
  packet(100+rate*131/2000); tick+=10;
  if(service_imu) MpuYaw_Task(tick,0);
  DriveBase_Task(tick);
}
static void reset(void)
{
  unsigned i;
  LineObstacleBypass_Stop(); GyroTurn_Stop(); DriveBase_Stop(DRIVE_STOP_COAST);
  tick=0; memset(counts,0,sizeof counts); memset(velocity,0,sizeof velocity);
  memset(fraction,0,sizeof fraction); memset(pins,0,sizeof pins);
  memset(registers,0,sizeof registers); registers[0x75]=0x68; fifo_size=0;
  no_yaw=stall_wheel=0; yaw_gain=12; saw_brake=saw_counter=saw_forward=0;
  DriveBase_Init(); assert(GyroTurn_ClearFault()); LineObstacleBypass_Init(0);
  MpuYaw_Init(tick); tick+=100; MpuYaw_Task(tick,1); tick+=100; MpuYaw_Task(tick,1);
  for(i=0;i<200;++i) { packet(100); tick+=10; MpuYaw_Task(tick,1); DriveBase_Task(tick); }
  assert(MpuYaw_IsReady(tick));
}
static void test_turn(int direction,int32_t gain)
{
  unsigned i;
  reset(); yaw_gain=gain;
  assert(GyroTurn_Start(direction*90000,2500));
  for(i=0;i<500 && GyroTurn_GetState()==GYRO_TURN_RUNNING;++i)
  {
    /* A consumer can be the first caller after an 80 ms main-loop delay. */
    if(i==40) { unsigned j; for(j=0;j<8;++j) plant(0); }
    else plant(1);
    GyroTurn_Task();
  }
  printf("real drive turn dir=%d gain=%ld state=%d fault=%u angle=%ld\n",
      direction,(long)gain,(int)GyroTurn_GetState(),GyroTurn_GetFault(),
      (long)GyroTurn_GetAchievedAngleMdeg());
  /* A more responsive yaw plant exposes recoil from the existing reverse
     brake pulse. The endpoint check must reject it, never claim success. */
  if(gain==20)
  {
    assert(GyroTurn_GetFault()==GYRO_TURN_ACCURACY);
    assert(GyroTurn_GetAchievedAngleMdeg()*direction<86000);
    assert(drive().mode==DRIVE_BASE_STOPPED && !GyroTurn_ClearTransientFault());
    return;
  }
  assert(GyroTurn_GetState()==GYRO_TURN_DONE && !GyroTurn_GetFault());
  assert(GyroTurn_GetAchievedAngleMdeg()*direction>=86000);
  assert(GyroTurn_GetAchievedAngleMdeg()*direction<=94000);
  assert(saw_brake && saw_counter && drive().mode==DRIVE_BASE_STOPPED);
  assert(LineBypassTravel_Start(40,1800));
  for(i=0;i<200 && LineBypassTravel_GetState()==LINE_BYPASS_TRAVEL_RUNNING;++i)
  { plant(1); LineBypassTravel_Task(); }
  assert(LineBypassTravel_GetState()==LINE_BYPASS_TRAVEL_DONE && saw_forward);
}
static void test_bypass(int direction,uint8_t interrupt_ir)
{
  unsigned i;
  LineObstacleBypassConfig config;
  LineObstacleBypassInput input={0};
  LineObstacleBypassTelemetry b;
  reset(); LineObstacleBypass_GetDefaultConfig(&config); config.turn_cps=2500;
  LineObstacleBypass_Init(&config);
  input.infrared_valid=1; input.left_ir_adc=input.right_ir_adc=3000;
  input.left_ir_threshold=input.right_ir_threshold=1700;
  input.left_ir_hysteresis=input.right_ir_hysteresis=20;
  assert(LineObstacleBypass_Start((int8_t)direction));
  for(i=0;i<700;++i)
  {
    plant(1); LineObstacleBypass_Task(&input); LineObstacleBypass_GetTelemetry(&b);
    assert(b.state!=LINE_BYPASS_FAULT);
    if(interrupt_ir && b.state==LINE_BYPASS_TURNING && saw_counter>10)
      input.left_ir_adc=input.right_ir_adc=1700;
    if(saw_counter && b.state==LINE_BYPASS_DRIVING && saw_forward) break;
  }
  assert(i<700 && saw_brake && saw_counter && saw_forward);
  assert(b.net_turn_mdeg*direction<0); /* Right bypass begins clockwise. */
  if(interrupt_ir) assert(b.net_turn_mdeg*direction>-40000);
  else assert(b.net_turn_mdeg*direction<=-41000);
  input.infrared_valid=0; LineObstacleBypass_Task(&input);
  assert(LineObstacleBypass_GetState()==LINE_BYPASS_FAULT);
  LineObstacleBypass_Stop(); DriveBase_Stop(DRIVE_STOP_COAST);
  for(i=0;i<30;++i) { plant(1); LineObstacleBypass_Task(&input); }
  assert(LineObstacleBypass_GetState()==LINE_BYPASS_IDLE && drive().mode==DRIVE_BASE_STOPPED);
  assert(pins[0]==0 && pins[1]==0 && pins[2]==0 && pins[3]==0);
}
static void test_faults(void)
{
  unsigned i;
  reset(); no_yaw=1; assert(GyroTurn_Start(90000,2500));
  for(i=0;i<150 && GyroTurn_GetState()==GYRO_TURN_RUNNING;++i) { plant(1); GyroTurn_Task(); }
  assert(saw_counter && GyroTurn_GetFault()==GYRO_TURN_NO_PROGRESS);
  assert(drive().mode==DRIVE_BASE_STOPPED);
  reset(); stall_wheel=3; assert(GyroTurn_Start(180000,2500));
  for(i=0;i<300 && GyroTurn_GetState()==GYRO_TURN_RUNNING;++i) { plant(1); GyroTurn_Task(); }
  assert(GyroTurn_GetFault()==GYRO_TURN_DRIVE && drive().fault_mask);
  reset(); assert(GyroTurn_Start(90000,2500));
  for(i=0;i<20;++i) { plant(1); GyroTurn_Task(); }
  GyroTurn_Stop();
  for(i=0;i<30;++i) { plant(1); GyroTurn_Task(); }
  assert(GyroTurn_GetState()==GYRO_TURN_IDLE && drive().mode==DRIVE_BASE_STOPPED);
}
static void test_automatic_recovery(void)
{
  unsigned i;
  LineWaitGuard guard={0};
  reset(); yaw_gain=20;
  assert(LineBypassTurn_Start(90000,2500) && LineBypassTurn_UsingGyro());
  for(i=0;i<500 && LineBypassTurn_GetState()==LINE_BYPASS_TURN_RUNNING;++i)
  { plant(1); LineBypassTurn_Task(); }
  assert(GyroTurn_GetFault()==GYRO_TURN_ACCURACY);
  /* Same ordered cancellation/acknowledgement boundary as the app. */
  assert(LineWaitGuard_Update(&guard,1,1,tick)==LINE_WAIT_NONE);
  for(i=0;i<80;++i) plant(1);
  assert(LineWaitGuard_Update(&guard,1,1,tick)==LINE_WAIT_BEGIN_RECOVERY);
  LineObstacleBypass_Stop(); DriveBase_Stop(DRIVE_STOP_COAST);
  DriveBase_ClearFault(); LineBypassTurn_Recover();
  assert(!GyroTurn_GetFault());
  LineWaitGuard_Drive(-1); plant(1); assert(drive().mode==DRIVE_BASE_SPEED);
  /* A manual STOP cancels the guard; no timer or later task restarts it. */
  LineWaitGuard_Reset(&guard); DriveBase_Stop(DRIVE_STOP_COAST);
  for(i=0;i<200;++i)
  { plant(1); assert(LineWaitGuard_Update(&guard,0,1,tick)==LINE_WAIT_NONE); LineBypassTurn_Task(); }
  assert(drive().mode==DRIVE_BASE_STOPPED);
  assert(LineBypassTurn_Start(-45000,2500) && !LineBypassTurn_UsingGyro());
  for(i=0;i<500 && LineBypassTurn_GetState()==LINE_BYPASS_TURN_RUNNING;++i)
  { plant(1); LineBypassTurn_Task(); }
  assert(LineBypassTurn_GetState()==LINE_BYPASS_TURN_DONE);
  LineBypassTurn_Stop();
  for(i=0;i<1100;++i) plant(1);
  assert(LineBypassTurn_Start(45000,2500) && LineBypassTurn_UsingGyro());
  LineBypassTurn_Stop();
  /* No calibrated IMU at all must still permit an encoder-estimated action. */
  MpuYaw_Init(tick);
  assert(LineBypassTurn_Start(45000,2500) && !LineBypassTurn_UsingGyro());
  for(i=0;i<500 && LineBypassTurn_GetState()==LINE_BYPASS_TURN_RUNNING;++i)
  { plant(1); LineBypassTurn_Task(); }
  assert(LineBypassTurn_GetState()==LINE_BYPASS_TURN_DONE);
  LineBypassTurn_Stop();
  puts("PASS: angle failure -> bounded recovery -> encoder fallback -> fresh gyro restoration; STOP cancels recovery");
}
static void test_return_cruise(int direction)
{
  unsigned i;
  LineObstacleBypassInput input={0};
  LineObstacleBypassTelemetry b;
  reset(); input.infrared_valid=1;
  input.left_ir_adc=input.right_ir_adc=1700;
  input.left_ir_threshold=input.right_ir_threshold=1700;
  input.left_ir_hysteresis=input.right_ir_hysteresis=20;
  assert(LineObstacleBypass_Start((int8_t)direction));
  for(i=0;i<4000;++i)
  {
    plant(1); LineObstacleBypass_Task(&input); LineObstacleBypass_GetTelemetry(&b);
    if(b.state==LINE_BYPASS_FAULT)
      printf("return failed i=%u yaw=%ld gyro=%u net=%ld\n",i,(long)b.return_yaw_mdeg,GyroTurn_GetFault(),(long)b.net_turn_mdeg);
    assert(b.state!=LINE_BYPASS_FAULT);
    if(b.flank_acquired && b.flank_travel_mm>=120) input.left_ir_adc=input.right_ir_adc=3000;
    if(b.return_cruise) break;
  }
  assert(i<4000 && b.return_yaw_valid && b.return_yaw_mdeg>45000);
  for(i=0;i<500;++i)
  {
    DriveBaseTelemetry d;
    plant(1); LineObstacleBypass_Task(&input); LineObstacleBypass_GetTelemetry(&b);
    d=drive();
    assert(b.return_cruise && b.state==LINE_BYPASS_DRIVING);
    assert(d.mode==DRIVE_BASE_SPEED && d.requested_cps[0]==1700 && d.requested_cps[2]==1700);
  }
  /* A queued edge followed by a wide transverse mark is not a rejoin. */
  ++tick; LineObstacleBypass_ObserveRawSensors(8,tick);
  ++tick; LineObstacleBypass_ObserveRawSensors(15,tick);
  ++tick; LineObstacleBypass_ObserveRawSensors(0,tick); LineObstacleBypass_Task(&input);
  assert(LineObstacleBypass_GetState()==LINE_BYPASS_DRIVING);
  /* An outer pulse between loop iterations survives the following white. */
  ++tick; LineObstacleBypass_ObserveRawSensors(direction>0 ? 8U : 2U,tick);
  ++tick; LineObstacleBypass_ObserveRawSensors(0,tick);
  LineObstacleBypass_Task(&input);
  assert(LineObstacleBypass_GetState()==LINE_BYPASS_DONE);
  assert(LineObstacleBypass_GetCapturedLineMask()==(direction>0 ? 1U : 8U));
  LineObstacleBypass_Stop();
  puts("PASS: full mirrored bypass reaches measured inward >45, drives continuously 5s and captures one-sample outer rejoin");
}
int main(void)
{
  setvbuf(stdout,0,_IONBF,0);
  test_turn(1,20); test_turn(-1,20); test_turn(1,12); test_turn(-1,12);
  test_bypass(1,0); test_bypass(-1,0); test_bypass(1,1); test_bypass(-1,1);
  test_faults();
  test_automatic_recovery();
  test_return_cruise(1); test_return_cruise(-1);
  puts("PASS: real DriveBase/FIFO/gyro/bypass chain, mirror turns, yaw gain, brake/travel ownership, IR interruption, stall and STOP");
  return 0;
}
