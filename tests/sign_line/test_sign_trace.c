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
  s.heading_error_mdeg=-12000; s.arc_peak_mdeg=160000;
  s.approach_from_pause=1;
  for(i=0;i<300;++i) { s.yaw_mdeg=(int32_t)i; SignTrace_Record(0xFFFFFF00U+i*20,12,&s,2200,0); }
  assert(!writes && SignTrace_Count()==256);
  assert(SignTrace_Get(0,&r) && r.yaw_mdeg==44);
  assert(r.heading_error_mdeg==-12000 && r.arc_peak_mdeg==160000 && r.left_cps==2200 && r.right_cps==0);
  assert(r.approach_from_pause==1);
  s.state=SIGN_ROUTE_CANCELLED; s.fault=3;
  SignTrace_Record(0xFFFFFF00U+299*20+1,0,&s,0,0); /* immediate transition, no 20ms wait */
  assert(SignTrace_Get(255,&r) && r.fault==3);
  s.state=SIGN_ROUTE_IDLE;
  for(i=0;i<100;++i) SignTrace_Record(i*20,6,&s,0,0);
  assert(SignTrace_Get(255,&r) && r.state==SIGN_ROUTE_CANCELLED);
  SignTrace_Request(0); SignTrace_Task(0); assert(!writes);
  SignTrace_Task(1); assert(writes==1);
  for(i=0;i<257;++i) SignTrace_Task(1);
  assert(SignTrace_Count()==256); /* dump and STOP preserve history */
  s.state=SIGN_ROUTE_ARMED; s.fault=0;
  SignTrace_Record(10000,6,&s,1412,1412);
  assert(SignTrace_Get(255,&r) && r.state==SIGN_ROUTE_ARMED && r.left_cps==1412);
  SignTrace_Request(1); SignTrace_Task(0); assert(SignTrace_Count()==256);
  SignTrace_Task(1); assert(!SignTrace_Count());
  puts("PASS: bounded trace, wrap, transition, freeze, STOP-only dump/clear, no motion-time UART");
  return 0;
}
