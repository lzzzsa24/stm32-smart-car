#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "mpu6050_yaw.h"
static uint8_t regs[256];
static int16_t gz;
uint8_t MpuBus_Init(void) { memset(regs,0,sizeof regs); regs[0x75]=0x68; return 1; }
uint8_t MpuBus_Write(uint8_t r,uint8_t v) { regs[r]=v; return 1; }
uint8_t MpuBus_Read(uint8_t r,uint8_t *d,uint16_t n)
{
  memset(d,0,n);
  if(r==0x72) d[1]=12;
  else if(r==0x74) { d[4]=0x40; d[10]=(uint8_t)((uint16_t)gz>>8); d[11]=(uint8_t)gz; }
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
  MpuYaw_Task(4300,0); assert(!MpuYaw_IsReady(4300));
  puts("PASS: actual MPU calibration, signed FIFO yaw integration and stale fault");
  return 0;
}
