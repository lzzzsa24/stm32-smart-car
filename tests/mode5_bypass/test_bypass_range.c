#include <assert.h>
#include <stdio.h>
#include "main.h"
#include "ultrasonic.h"
#include "line_bypass_range.h"

static uint32_t tick;
static uint16_t distance;
static uint8_t result, busy, start_ok;
static unsigned starts;
uint32_t HAL_GetTick(void) { return tick; }
void Ultrasonic_Task(void) {}
uint8_t Ultrasonic_IsBusy(void) { return busy; }
uint8_t Ultrasonic_Start(void)
{ if(!start_ok) return 0; assert(!busy); busy=1; ++starts; return 1; }
uint8_t Ultrasonic_GetResult(uint16_t *cm)
{ uint8_t r=result; *cm=distance; result=0; return r; }
static uint8_t step(uint8_t forward,uint32_t ms)
{ tick+=ms; return LineBypassRange_Task(forward,15); }
static void echo(uint16_t cm)
{ busy=0; result=ULTRASONIC_RESULT_OK; distance=cm; }
static void reset(uint32_t now)
{ tick=now; result=busy=0; starts=0; start_ok=1; LineBypassRange_Reset(); }

int main(void)
{
  unsigned n;
  reset(100);
  echo(5); assert(!step(0,0) && !starts); /* Original obstacle during turn. */
  for(n=0;n<30;++n) assert(!step(0,10) && !starts);
  assert(!step(1,0) && starts==1);
  echo(16); assert(!step(1,10));
  assert(!step(1,50) && starts==2);
  echo(15); assert(step(1,10)); /* Fresh forward shot still interrupts. */
  assert(!step(0,1)); /* Drop near latch immediately on turning. */
  assert(!step(1,120) && starts==3); /* Not held for the old 250-ms interval. */
  /* In-flight shot from the previous heading arrives on the new leg. */
  assert(!step(0,1)); assert(!step(1,1));
  echo(3); assert(!step(1,10) && starts==4);
  echo(3); assert(step(1,10)); /* New heading's shot is authoritative. */
  assert(!step(1,251)); /* Unrefreshed result expires. */

  reset(100); assert(!step(1,0));
  LineBypassRange_Reset(); echo(4); assert(!step(1,10));
  echo(40); assert(!step(1,10));
  assert(!step(1,50)); echo(4); assert(!step(1,251)); /* late shot rejected */
  reset(100); start_ok=0; assert(!step(1,0) && !starts);
  echo(4); assert(!step(1,10));
  start_ok=1; assert(!step(1,1)); echo(4); assert(step(1,10));
  reset(UINT32_MAX-40); assert(!step(1,0));
  echo(15); assert(step(1,50)); assert(!step(0,1));
  puts("PASS: turn-heading echoes and in-flight old shots cannot truncate the next leg; fresh 15-cm guard, reset, expiry and wrap retained");
  return 0;
}
