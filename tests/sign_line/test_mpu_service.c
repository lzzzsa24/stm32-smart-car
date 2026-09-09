#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "mpu6050_yaw.h"
static uint8_t regs[256];
static int16_t gz;
static int16_t gx, gy, ax, az=16384;
static uint8_t empty;
static void put(uint8_t *d, int16_t value)
{ d[0]=(uint8_t)((uint16_t)value>>8); d[1]=(uint8_t)value; }
uint8_t MpuBus_Init(void) { memset(regs,0,sizeof regs); regs[0x75]=0x68; return 1; }
uint8_t MpuBus_Write(uint8_t r,uint8_t v) { regs[r]=v; return 1; }
uint8_t MpuBus_Read(uint8_t r,uint8_t *d,uint16_t n)
{
  memset(d,0,n);
  if(r==0x72) d[1]=empty ? 0 : 12;
  else if(r==0x74) { put(d,ax); put(d+4,az); put(d+6,gx); put(d+8,gy); put(d+10,gz); }
  else d[0]=regs[r];
  return 1;
}
int main(void)
{
  unsigned t; MpuYawReading r;
  MpuYaw_Init(0); MpuYaw_Task(100,1); MpuYaw_Task(200,1);
  for(t=210;t<=2200;t+=10) MpuYaw_Task(t,1);
  assert(MpuYaw_IsReady(2200));
  gz=6550;
  for(t=2210;t<=3200;t+=10) MpuYaw_Task(t,0);
  MpuYaw_GetReading(&r); assert(r.yaw_mdeg==100000);
  gz=-6550;
  for(t=3210;t<=4200;t+=10) MpuYaw_Task(t,0);
  MpuYaw_GetReading(&r); assert(r.yaw_mdeg==0);
  MpuYaw_Task(4300,0); assert(MpuYaw_IsReady(4300));
  empty=1; MpuYaw_Task(4331,0); assert(!MpuYaw_IsReady(4331));
  MpuYaw_GetReading(&r); assert(!r.fault);
  MpuYaw_Task(4551,0); MpuYaw_GetReading(&r); assert(r.fault==MPU_FAULT_STALE);
  puts("PASS: actual MPU calibration, signed FIFO yaw integration and stale fault");
  empty=0; gx=400; gy=-500; gz=600;
  MpuYaw_Init(0); MpuYaw_Task(100,1); MpuYaw_Task(200,1);
  for(t=210;t<=2200;t+=10) MpuYaw_Task(t,1);
  MpuYaw_GetReading(&r);
  assert(MpuYaw_IsReady(2200) && r.bias_milliraw==600000);
  for(t=2210;t<=3200;t+=10) MpuYaw_Task(t,1);
  MpuYaw_GetReading(&r); assert(r.rate_mdeg_s==0 && r.yaw_mdeg==0);
  assert(r.raw_gyro[0]==400 && r.raw_gyro[1]==-500 && r.raw_accel[2]==16384);
  gz=1255;
  for(t=3210;t<=4200;t+=10) MpuYaw_Task(t,0);
  MpuYaw_GetReading(&r); assert(r.yaw_mdeg==10000);
  gz=-600; gx=-400; gy=500;
  MpuYaw_Init(0); MpuYaw_Task(100,1); MpuYaw_Task(200,1);
  for(t=210;t<=2200;t+=10) MpuYaw_Task(t,1);
  MpuYaw_GetReading(&r); assert(MpuYaw_IsReady(2200) && r.bias_milliraw==-600000);
  MpuYaw_Init(0); MpuYaw_Task(100,1); MpuYaw_Task(200,1);
  for(t=210;t<=3200;t+=10) { gz=(t/10)%2 ? 400:500; MpuYaw_Task(t,1); }
  MpuYaw_GetReading(&r); assert(!MpuYaw_IsReady(3200));
  assert(r.cal_last_reject==MPU_CAL_UNSTABLE && r.cal_rejections>0);
  gz=1311; MpuYaw_Task(3210,1); MpuYaw_GetReading(&r);
  assert(r.cal_reject==MPU_CAL_RAW_LIMIT && r.calibration_samples==0);
  gz=0; ax=5000; MpuYaw_Task(3220,1); MpuYaw_GetReading(&r);
  assert(r.cal_reject==MPU_CAL_TILT);
  ax=0; az=-16384; MpuYaw_Task(3230,1); MpuYaw_GetReading(&r);
  assert(r.cal_reject==MPU_CAL_Z);
  az=16384; MpuYaw_Task(3240,0); MpuYaw_GetReading(&r);
  assert(r.state==MPU_YAW_WAIT_STATIONARY && !MpuYaw_IsReady(3240));
  puts("PASS: signed DC bias over old limit calibrates, corrected yaw, unstable/tilted/inverted/outlier rejection and STOP requirement");
  return 0;
}
