#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "sign_trace.h"
static unsigned writes;
void DiagnosticUart_WriteString(const char *s) { (void)s; ++writes; }
void DiagnosticUart_WriteSigned(int32_t v) { (void)v; ++writes; }
void DiagnosticUart_WriteUnsigned(uint32_t v) { (void)v; ++writes; }
int main(void)
{
  SignRouteStatus s={0}; SignTraceRecord r; unsigned i;
  SignTrace_Init(); s.state=SIGN_ROUTE_ARC;
  for(i=0;i<300;++i) { s.yaw_mdeg=(int32_t)i; SignTrace_Record(0xFFFFFF00U+i*20,12,&s); }
  assert(!writes && SignTrace_Count()==256);
  assert(SignTrace_Get(0,&r) && r.yaw_mdeg==44);
  s.state=SIGN_ROUTE_CANCELLED; s.fault=3;
  SignTrace_Record(0xFFFFFF00U+299*20+1,0,&s); /* immediate transition, no 20ms wait */
  assert(SignTrace_Get(255,&r) && r.fault==3);
  s.state=SIGN_ROUTE_IDLE;
  for(i=0;i<100;++i) SignTrace_Record(i*20,6,&s);
  assert(SignTrace_Get(255,&r) && r.state==SIGN_ROUTE_CANCELLED);
  SignTrace_Request(0); SignTrace_Task(0); assert(!writes);
  SignTrace_Task(1); assert(writes==1);
  for(i=0;i<257;++i) SignTrace_Task(1);
  assert(SignTrace_Count()==256); /* dump and STOP preserve history */
  SignTrace_Request(1); SignTrace_Task(0); assert(SignTrace_Count()==256);
  SignTrace_Task(1); assert(!SignTrace_Count());
  puts("PASS: bounded trace, wrap, transition, freeze, STOP-only dump/clear, no motion-time UART");
  return 0;
}
