/* White-box boundary injection into the production bypass FSM. Real-drive
   reachability is covered separately by test_real_drive.c. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "drive_base.h"
#include "line_bypass_travel.h"
#include "line_bypass_turn.h"
#include "mpu6050_yaw.h"
static uint32_t test_ms;
static DriveBaseTelemetry test_drive;
static MpuYawReading test_imu;
static uint8_t test_ready;
static LineBypassTravelState travel_state;
static LineBypassTurnState turn_state;
static int32_t turn_request, turn_achieved, travel_request;
static uint32_t travel_progress;
static unsigned turn_calls, travel_calls;
uint32_t HAL_GetTick(void) { return test_ms; }
void MpuYaw_Refresh(uint32_t t) { (void)t; }
void MpuYaw_GetReading(MpuYawReading *r) { *r=test_imu; }
uint8_t MpuYaw_IsReady(uint32_t t) { (void)t; return test_ready; }
void DriveBase_GetTelemetry(DriveBaseTelemetry *d) { *d=test_drive; }
void DriveBase_Stop(DriveStopMode m) { (void)m; test_drive.mode=DRIVE_BASE_STOPPED; }
void DriveBase_SetLineFaultObservation(uint8_t e,uint8_t s,uint8_t r) { (void)e;(void)s;(void)r; }
void DriveBase_SetSideCps(int32_t l,int32_t r)
{ test_drive.mode=DRIVE_BASE_SPEED; test_drive.requested_cps[0]=l; test_drive.requested_cps[2]=r; }
void advanced_stop(void) { DriveBase_Stop(DRIVE_STOP_COAST); }
void LineBypassTravel_Stop(void) { travel_state=LINE_BYPASS_TRAVEL_IDLE; }
uint8_t LineBypassTravel_Start(int32_t mm,int32_t cps)
{ travel_request=mm; travel_progress=0; ++travel_calls; travel_state=LINE_BYPASS_TRAVEL_RUNNING; DriveBase_SetSideCps(cps,cps); return 1; }
void LineBypassTravel_Task(void) {}
LineBypassTravelState LineBypassTravel_GetState(void) { return travel_state; }
uint32_t LineBypassTravel_GetProgressMm(void) { return travel_progress; }
uint8_t LineBypassTravel_GetFaultMask(void) { return 0; }
void LineBypassTurn_Stop(void) { turn_state=LINE_BYPASS_TURN_IDLE; }
uint8_t LineBypassTurn_Start(int32_t a,int32_t cps)
{ turn_request=a; ++turn_calls; turn_achieved=0; turn_state=LINE_BYPASS_TURN_RUNNING; DriveBase_SetSideCps(a>0?-cps:cps,a>0?cps:-cps); return 1; }
void LineBypassTurn_Task(void) {}
uint8_t LineBypassTurn_RequestStop(void) { return 1; }
LineBypassTurnState LineBypassTurn_GetState(void) { return turn_state; }
int32_t LineBypassTurn_GetAchievedAngleMdeg(void) { return turn_achieved; }
uint8_t LineBypassTurn_GetFaultMask(void) { return 0; }
#include "../../Core/Src/line_obstacle_bypass.c"

static LineObstacleBypassInput setup(int direction)
{
  LineObstacleBypassInput input={0};
  test_ms=1000; memset(&test_drive,0,sizeof test_drive); memset(&test_imu,0,sizeof test_imu);
  test_ready=1; test_imu.yaw_mdeg=1230000; test_imu.generation=7;
  LineObstacleBypass_Init(0); assert(LineObstacleBypass_Start((int8_t)direction));
  input.infrared_valid=1; input.left_ir_adc=input.right_ir_adc=3000;
  input.left_ir_threshold=input.right_ir_threshold=1700;
  input.left_ir_hysteresis=input.right_ir_hysteresis=20;
  /* Snapshot reached after clearing the original line and rounding the rear. */
  original_line_cleared=1; return_phase_active=return_aligned=1;
  active_drive_intent=BYPASS_INTENT_RETURN_TO_LINE; bypass_state=LINE_BYPASS_DRIVING;
  DriveBase_SetSideCps(1800,1800); LineObstacleBypass_Task(&input);
  return input;
}
static void angle(LineObstacleBypassInput *input,int direction,int32_t value)
{
  test_ms+=10; test_imu.yaw_mdeg=1230000+(int64_t)direction*value;
  LineObstacleBypass_Task(input);
}
static LineObstacleBypassInput fixed_setup(int direction)
{
  LineObstacleBypassInput input={0};
  LineObstacleBypassConfig config;
  test_ms=1000; memset(&test_drive,0,sizeof test_drive); memset(&test_imu,0,sizeof test_imu);
  test_ready=1; test_imu.yaw_mdeg=1230000; test_imu.generation=7;
  turn_calls=travel_calls=0; turn_achieved=turn_request=travel_request=0; travel_progress=0;
  LineObstacleBypass_GetDefaultConfig(&config); config.fixed_route_direction=(int8_t)direction;
  config.return_cps=2300; config.turn_cps=2500;
  LineObstacleBypass_Init(&config); assert(LineObstacleBypass_Start((int8_t)-direction));
  input.infrared_valid=1; input.left_ir_adc=input.right_ir_adc=3000;
  input.left_ir_threshold=input.right_ir_threshold=1700;
  input.left_ir_hysteresis=input.right_ir_hysteresis=20;
  return input;
}
static void fixed_step(LineObstacleBypassInput *input)
{
  if(bypass_state==LINE_BYPASS_TURNING)
  {
    turn_achieved=turn_request; test_imu.yaw_mdeg+=turn_achieved;
    turn_state=LINE_BYPASS_TURN_DONE; test_drive.mode=DRIVE_BASE_STOPPED;
  }
  else if(bypass_state==LINE_BYPASS_DRIVING && fixed_phase!=LINE_FIXED_RETURN)
  {
    travel_progress=(uint32_t)travel_request;
    travel_state=LINE_BYPASS_TRAVEL_DONE; test_drive.mode=DRIVE_BASE_STOPPED;
  }
  test_ms+=120; LineObstacleBypass_Task(input);
}
static void test_fixed_route(void)
{
  int direction;
  unsigned phase,obstacle,i;
  for(direction=-1;direction<=1;direction+=2)
  {
    LineObstacleBypassInput input=fixed_setup(direction);
    /* No turn before the nonblocking entry wait has ended. */
    test_ms+=119; LineObstacleBypass_Task(&input); assert(turn_calls==0);
    ++test_ms; LineObstacleBypass_Task(&input);
    assert(turn_request==-90000*direction && turn_calls==1);
    fixed_step(&input); assert(travel_request==300 && travel_calls==1);
    /* Simulate real yaw drift during the first straight. */
    test_imu.yaw_mdeg+=3000*direction;
    fixed_step(&input); assert(turn_request==87000*direction);
    fixed_step(&input); assert(travel_request==360 && travel_calls==2);
    test_imu.yaw_mdeg-=2000*direction;
    fixed_step(&input); assert(turn_request==47000*direction && turn_calls==3);
    fixed_step(&input); assert(fixed_phase==LINE_FIXED_RETURN && return_cruise);
    for(i=0;i<100;++i) { ++test_ms; LineObstacleBypass_Task(&input); }
    assert(turn_calls==3 && travel_calls==2 && test_drive.requested_cps[0]==2100);

    /* STOP cancels every phase, including the continuous diagonal. */
    for(phase=LINE_FIXED_ENTRY;phase<=LINE_FIXED_RETURN;++phase)
    {
      input=fixed_setup(direction);
      while((unsigned)fixed_phase<phase) fixed_step(&input);
      LineObstacleBypass_Stop();
      for(i=0;i<10;++i) { ++test_ms; LineObstacleBypass_Task(&input); }
      assert(bypass_state==LINE_BYPASS_IDLE && fixed_phase==LINE_FIXED_NONE);
      assert(test_drive.mode==DRIVE_BASE_STOPPED);
    }
    /* New front or close inside boundary on each straight abandons the
       rectangle once, preserving the current pose for adaptive avoidance. */
    for(phase=LINE_FIXED_OFFSET;phase<=LINE_FIXED_RETURN;phase+=2)
      for(obstacle=0;obstacle<2;++obstacle)
      {
        input=fixed_setup(direction);
        while((unsigned)fixed_phase<phase) fixed_step(&input);
        if(obstacle) input.left_ir_adc=input.right_ir_adc=1000;
        else input.front_obstacle=1;
        ++test_ms; LineObstacleBypass_Task(&input);
        assert(fixed_phase==LINE_FIXED_NONE && fixed_fallback && bypass_state==LINE_BYPASS_TURNING);
        assert(!return_cruise);
        if(!obstacle)
        {
          unsigned old_travel_calls=travel_calls;
          fixed_step(&input); /* Complete escape turn, still blocked in front. */
          ++test_ms; LineObstacleBypass_Task(&input);
          assert(bypass_state==LINE_BYPASS_TURNING && travel_calls==old_travel_calls);
        }
      }
    /* Generation changes invalidate absolute yaw: use accumulated achieved
       angle, not the new IMU origin. */
    input=fixed_setup(direction); fixed_step(&input); fixed_step(&input);
    ++test_imu.generation; test_imu.yaw_mdeg+=500000;
    fixed_step(&input); assert(!return_yaw_valid && turn_request==90000*direction);
    input=fixed_setup(direction); test_ready=0;
    for(i=0;i<6;++i) fixed_step(&input);
    assert(fixed_phase==LINE_FIXED_RETURN && !return_yaw_valid);

    /* A pre-return queued edge cannot finish the route; a fresh one during
       the final turn can, after leaving the original line. */
    input=fixed_setup(direction);
    while(fixed_phase<LINE_FIXED_PARALLEL) fixed_step(&input);
    original_line_cleared=1;
    ++test_ms; LineObstacleBypass_ObserveRawSensors(8,test_ms);
    fixed_step(&input); assert(fixed_phase==LINE_FIXED_RETURN_TURN && !capture_mask);
    ++test_ms; LineObstacleBypass_ObserveRawSensors(8,test_ms);
    ++test_ms; LineObstacleBypass_ObserveRawSensors(0,test_ms); LineObstacleBypass_Task(&input);
    assert(bypass_state==LINE_BYPASS_DONE && capture_mask==1);

    input=fixed_setup(direction); test_ms=UINT32_MAX-50;
    LineObstacleBypass_Stop(); assert(LineObstacleBypass_Start((int8_t)direction));
    for(i=0;i<6;++i) fixed_step(&input);
    assert(fixed_phase==LINE_FIXED_RETURN);
    test_drive.fault_mask=1; ++test_ms; LineObstacleBypass_Task(&input);
    assert(bypass_state==LINE_BYPASS_FAULT);
    LineObstacleBypass_Stop(); test_drive.fault_mask=0;
  }
  puts("PASS: fixed 300/360/45 geometry, absolute-heading drift correction, all-phase STOP, obstacle fallback, generation/encoder fallback, capture gate and timer wrap");
}

static void test_ir_disabled(void)
{
  int direction;
  unsigned i;
  for(direction=-1;direction<=1;direction+=2)
  {
    LineObstacleBypassInput input=fixed_setup(direction), snapshot;
    LineObstacleBypassTelemetry telemetry;
    bypass_config.infrared_enabled=0;
    input.infrared_valid=0;
    input.left_ir_adc=input.right_ir_adc=0;
    input.left_ir_threshold=input.right_ir_threshold=4095;
    input.left_ir_hysteresis=input.right_ir_hysteresis=65535;
    snapshot=input;
    for(i=0;i<6;++i) fixed_step(&input);
    assert(fixed_phase==LINE_FIXED_RETURN && !fixed_fallback);
    assert(turn_calls==3 && travel_calls==2 && fault_mask==0);
    assert(memcmp(&input,&snapshot,sizeof input)==0);
    for(i=0;i<20;++i) { ++test_ms; LineObstacleBypass_Task(&input); }
    assert(fixed_phase==LINE_FIXED_RETURN && !fixed_fallback);
    LineObstacleBypass_GetTelemetry(&telemetry); assert(!telemetry.infrared_enabled);
    /* Disabling side IR cannot suppress front ultrasonic or STOP. */
    input.front_obstacle=1; ++test_ms; LineObstacleBypass_Task(&input);
    assert(fixed_fallback && bypass_state==LINE_BYPASS_TURNING);
    fixed_step(&input); ++test_ms; LineObstacleBypass_Task(&input);
    assert(bypass_state==LINE_BYPASS_TURNING && travel_calls==2);
    LineObstacleBypass_Stop(); ++test_ms; LineObstacleBypass_Task(&input);
    assert(bypass_state==LINE_BYPASS_IDLE && test_drive.mode==DRIVE_BASE_STOPPED);
    /* A null input is still invalid, rather than invented line/front data. */
    input=fixed_setup(direction); bypass_config.infrared_enabled=0;
    LineObstacleBypass_Task(0); assert(bypass_state==LINE_BYPASS_FAULT);
    LineObstacleBypass_Stop();
  }
  puts("PASS: IR-off ignores invalid/near/saturated samples through fixed and adaptive phases, preserves input, ultrasonic and STOP");
}

int main(void)
{
  int dir;
  unsigned obstacle;
  for(dir=-1;dir<=1;dir+=2)
  {
    LineObstacleBypassInput input=setup(dir);
    angle(&input,dir,44999); assert(!return_cruise);
    angle(&input,dir,45000); assert(!return_cruise);
    angle(&input,dir,45001); assert(return_cruise);
    assert(test_drive.requested_cps[0]==1700 && test_drive.requested_cps[2]==1700);
    bypass_config.return_cps=2300; angle(&input,dir,46000);
    assert(test_drive.requested_cps[0]==2100 && test_drive.requested_cps[2]==2100);
    bypass_config.return_cps=1900; angle(&input,dir,46000);
    assert(test_drive.requested_cps[0]==1900 && test_drive.requested_cps[2]==1900);
    bypass_config.return_cps=900; angle(&input,dir,46000);
    assert(test_drive.requested_cps[0]==1412 && test_drive.requested_cps[2]==1412);
    for(obstacle=1;obstacle<=3;++obstacle)
    {
      input=setup(dir);
      if(obstacle==1) input.front_obstacle=1;
      if(obstacle==2) input.left_ir_adc=1000;
      if(obstacle==3) input.right_ir_adc=1000;
      angle(&input,dir,45001); assert(!return_cruise);
      input.front_obstacle=0; input.left_ir_adc=input.right_ir_adc=3000;
      angle(&input,dir,45001); assert(return_cruise);
      if(obstacle==1) input.front_obstacle=1;
      if(obstacle==2) input.left_ir_adc=1000;
      if(obstacle==3) input.right_ir_adc=1000;
      angle(&input,dir,45001); assert(!return_cruise && bypass_state==LINE_BYPASS_TURNING);
    }
    input=setup(dir); test_ready=0; angle(&input,dir,60000); assert(!return_cruise);
    input=setup(dir); ++test_imu.generation; angle(&input,dir,60000); assert(!return_cruise);
    input=setup(dir); angle(&input,dir,-60000); assert(!return_cruise);
    input=setup(dir); return_phase_active=0; angle(&input,dir,60000); assert(!return_cruise);
    input=setup(dir); test_ms+=1; LineObstacleBypass_ObserveRawSensors(8,test_ms);
    test_ms+=101; LineObstacleBypass_Task(&input); assert(bypass_state!=LINE_BYPASS_DONE);
    input=setup(dir); original_line_cleared=0; ++test_ms;
    LineObstacleBypass_ObserveRawSensors(8,test_ms); LineObstacleBypass_Task(&input);
    assert(bypass_state!=LINE_BYPASS_DONE);
    input=setup(dir); ++test_ms; LineObstacleBypass_ObserveRawSensors(8,test_ms);
    LineObstacleBypass_Stop(); ++test_ms; LineObstacleBypass_Task(&input);
    assert(bypass_state==LINE_BYPASS_IDLE && test_drive.mode==DRIVE_BASE_STOPPED);
  }
  puts("PASS: 44.999/45/45.001 degree mirrored gate, all obstacle inputs, missing/reset yaw, outward phase, expired contact and STOP");
  test_fixed_route();
  test_ir_disabled();
  return 0;
}
