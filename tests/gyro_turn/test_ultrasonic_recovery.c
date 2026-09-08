#include <assert.h>
#include <stdio.h>
#include "main.h"
#include "ultrasonic.h"
#include "ultrasonic_avoid.h"
static uint32_t tick;
static uint8_t result;
static uint16_t cm;
static int16_t output;
uint32_t HAL_GetTick(void) { return tick; }
void Ultrasonic_Task(void) {}
uint8_t Ultrasonic_Start(void) { return 1; }
uint8_t Ultrasonic_IsBusy(void) { return 0; }
uint8_t Ultrasonic_GetResult(uint16_t *distance) { *distance=cm; return result; }
uint16_t Ultrasonic_GetLastDistanceMm(void) { return (uint16_t)(cm*10); }
static void drive(int16_t left,int16_t right) { assert(left==right); output=left; }
static void stop(void) { output=0; }
static void check(uint8_t failed_result,uint32_t start)
{
  unsigned i;
  tick=start; result=failed_result; cm=0;
  UltrasonicAvoid_Init(drive,stop,0,0);
  UltrasonicAvoid_SetSpeeds(2800,1800,1800,2400);
  UltrasonicAvoid_SetNoEchoFallback(1,3);
  for(i=0;i<70;++i) { tick+=10; UltrasonicAvoid_Task(); }
  assert(UltrasonicAvoid_IsNoEchoFallbackActive() && output==1800);
  for(i=0;i<100;++i) { tick+=10; UltrasonicAvoid_Task(); assert(output==1800); }
  result=ULTRASONIC_RESULT_OK; cm=3; tick+=10; UltrasonicAvoid_Task();
  assert(!UltrasonicAvoid_IsNoEchoFallbackActive());
  assert(UltrasonicAvoid_GetState()==ULTRASONIC_AVOID_STOPPING && output==0);
}
int main(void)
{
  check(ULTRASONIC_RESULT_NONE,100);
  check(ULTRASONIC_RESULT_OUT_RANGE,100);
  check(ULTRASONIC_RESULT_TIMEOUT,UINT32_MAX-100);
  puts("PASS: no-result/out-of-range/timeout bounded slow fallback, wrap and immediate fresh close-obstacle priority");
  return 0;
}
