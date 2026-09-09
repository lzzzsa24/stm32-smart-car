#include <assert.h>
#include <stdio.h>
#include "main.h"
#include "ultrasonic.h"
#include "ultrasonic_avoid.h"
static uint32_t tick;
static uint8_t result;
static uint16_t cm;
static int16_t output;
static unsigned stop_count;
uint32_t HAL_GetTick(void) { return tick; }
void Ultrasonic_Task(void) {}
uint8_t Ultrasonic_Start(void) { return 1; }
uint8_t Ultrasonic_IsBusy(void) { return 0; }
uint8_t Ultrasonic_GetResult(uint16_t *distance) { *distance=cm; return result; }
uint16_t Ultrasonic_GetLastDistanceMm(void) { return (uint16_t)(cm*10); }
static void drive(int16_t left,int16_t right) { assert(left==right); output=left; }
static void stop(void) { output=0; ++stop_count; }
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
static void send(uint8_t status,uint16_t distance,uint32_t elapsed)
{ result=status; cm=distance; tick+=elapsed; UltrasonicAvoid_Task(); }
static void start_clear(void)
{
  tick=100; UltrasonicAvoid_Init(drive,stop,0,0);
  UltrasonicAvoid_SetSpeeds(2800,1800,1800,2400);
  UltrasonicAvoid_SetNoEchoFallback(1,3);
  send(ULTRASONIC_RESULT_OK,100,60); send(ULTRASONIC_RESULT_OK,100,60);
  send(ULTRASONIC_RESULT_OK,100,60);
  assert(UltrasonicAvoid_GetState()==ULTRASONIC_AVOID_FORWARD && output==2800);
  stop_count=0;
}
static void test_continuity(void)
{
  unsigned i;
  start_clear();
  send(ULTRASONIC_RESULT_OUT_RANGE,0,60);
  assert(stop_count==0 && output==1800);
  /* An isolated good echo must not release the degraded cap, nor re-enter
     WAIT_SAFE while the median filter is still empty. */
  for(i=0;i<100;++i)
  {
    send(ULTRASONIC_RESULT_OK,100,60);
    assert(output==1800 && stop_count==0);
    send(i%2 ? ULTRASONIC_RESULT_TIMEOUT : ULTRASONIC_RESULT_OUT_RANGE,0,60);
    assert(output==1800 && stop_count==0);
    send(ULTRASONIC_RESULT_NONE,0,10);
    assert(UltrasonicAvoid_GetState()==ULTRASONIC_AVOID_FORWARD);
  }
  send(ULTRASONIC_RESULT_OK,100,60); assert(output==1800);
  send(ULTRASONIC_RESULT_NONE,0,1); assert(output==1800);
  send(ULTRASONIC_RESULT_OK,100,59); assert(output==1800);
  send(ULTRASONIC_RESULT_OK,100,60); assert(output==2800 && !stop_count);
  /* Driver stalls without producing a TIMEOUT packet. */
  send(ULTRASONIC_RESULT_NONE,0,251); assert(output==1800 && !stop_count);
  send(ULTRASONIC_RESULT_OK,20,60); assert(!stop_count);
  send(ULTRASONIC_RESULT_TIMEOUT,0,60); assert(!stop_count);
  send(ULTRASONIC_RESULT_OK,20,60); assert(!stop_count); /* not consecutive */
  send(ULTRASONIC_RESULT_OK,20,60);
  assert(UltrasonicAvoid_GetState()==ULTRASONIC_AVOID_STOPPING && stop_count);
  start_clear(); send(ULTRASONIC_RESULT_OUT_RANGE,0,60);
  send(ULTRASONIC_RESULT_OK,3,60);
  assert(UltrasonicAvoid_GetState()==ULTRASONIC_AVOID_STOPPING && output==0);
  puts("PASS: noisy-echo continuity, three-frame recovery, stale-without-result, near-confirmation gaps and critical priority");
  start_clear(); send(ULTRASONIC_RESULT_OK,20,60);
  send(ULTRASONIC_RESULT_OK,20,300); assert(!stop_count);
  send(ULTRASONIC_RESULT_OK,20,60);
  assert(UltrasonicAvoid_GetState()==ULTRASONIC_AVOID_STOPPING && stop_count);
  start_clear(); UltrasonicAvoid_SetNoEchoFallback(0,3);
  send(ULTRASONIC_RESULT_OUT_RANGE,0,60);
  assert(UltrasonicAvoid_GetState()==ULTRASONIC_AVOID_WAIT_SAFE && stop_count);
}
int main(void)
{
  start_clear();
  send(ULTRASONIC_RESULT_OK,3,60);
  stop_count=0;
  UltrasonicAvoid_ResumeFollowing();
  assert(!stop_count && output==1800 && UltrasonicAvoid_GetState()==ULTRASONIC_AVOID_FORWARD);
  send(ULTRASONIC_RESULT_OK,3,60);
  assert(stop_count && UltrasonicAvoid_GetState()==ULTRASONIC_AVOID_STOPPING);
  check(ULTRASONIC_RESULT_NONE,100);
  check(ULTRASONIC_RESULT_OUT_RANGE,100);
  check(ULTRASONIC_RESULT_TIMEOUT,UINT32_MAX-100);
  test_continuity();
  start_clear(); UltrasonicAvoid_SetThresholds(16,28); UltrasonicAvoid_SetEmergencyDistance(22);
  send(ULTRASONIC_RESULT_OK,23,60); assert(!stop_count);
  send(ULTRASONIC_RESULT_OK,22,60); assert(!stop_count && output==1800);
  send(ULTRASONIC_RESULT_OK,21,60);
  assert(stop_count && UltrasonicAvoid_GetState()==ULTRASONIC_AVOID_STOPPING);
  start_clear(); UltrasonicAvoid_SetThresholds(16,28); UltrasonicAvoid_SetEmergencyDistance(22);
  send(ULTRASONIC_RESULT_OK,11,60); assert(!stop_count);
  start_clear(); UltrasonicAvoid_SetThresholds(16,28); UltrasonicAvoid_SetEmergencyDistance(22);
  send(ULTRASONIC_RESULT_OK,10,60);
  assert(stop_count && UltrasonicAvoid_GetState()==ULTRASONIC_AVOID_STOPPING);
  puts("PASS: 16/28-cm approach profile, two fresh echoes at 22 cm and immediate raw 10-cm stop");
  puts("PASS: no-result/out-of-range/timeout bounded slow fallback, wrap and immediate fresh close-obstacle priority");
  return 0;
}
