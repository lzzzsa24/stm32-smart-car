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
void LineBypassTravel_Stop(void) {}
uint8_t LineBypassTravel_Start(int32_t mm,int32_t cps) { (void)mm; DriveBase_SetSideCps(cps,cps); return 1; }
uint8_t LineBypassTravel_StartFixed(int32_t mm,int32_t cps) { return LineBypassTravel_Start(mm,cps); }
void LineBypassTravel_Task(void) {}
LineBypassTravelState LineBypassTravel_GetState(void) { return LINE_BYPASS_TRAVEL_RUNNING; }
uint32_t LineBypassTravel_GetProgressMm(void) { return 0; }
uint8_t LineBypassTravel_GetFaultMask(void) { return 0; }
void LineBypassTurn_Stop(void) {}
uint8_t LineBypassTurn_Start(int32_t a,int32_t cps) { (void)a;(void)cps; return 1; }
void LineBypassTurn_Task(void) {}
uint8_t LineBypassTurn_RequestStop(void) { return 1; }
LineBypassTurnState LineBypassTurn_GetState(void) { return LINE_BYPASS_TURN_RUNNING; }
int32_t LineBypassTurn_GetAchievedAngleMdeg(void) { return 0; }
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
  return 0;
}
