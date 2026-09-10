#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "promoted_sign_trace.h"
static unsigned writes;
void DiagnosticUart_WriteString(const char *s) { (void)s; ++writes; }
void DiagnosticUart_WriteSigned(int32_t v) { (void)v; ++writes; }
void DiagnosticUart_WriteUnsigned(uint32_t v) { (void)v; ++writes; }
int main(void)
{
  Promoted_SignRouteStatus s={0}; Promoted_SignTraceRecord r; unsigned i;
  Promoted_SignTrace_Init(); s.state=Promoted_SIGN_ROUTE_ARC;
  s.heading_error_mdeg=-12000; s.arc_peak_mdeg=160000;
  s.approach_from_pause=1;
  s.exit_heading_peak_mdeg=55000;
  s.road_reference_valid=1; s.exit_reason=2; s.arc_sweep_mdeg=130000;
  for(i=0;i<300;++i) { s.yaw_mdeg=(int32_t)i; Promoted_SignTrace_Record(0xFFFFFF00U+i*20,12,&s,2200,0,3,6,0); }
  assert(!writes && Promoted_SignTrace_Count()==256);
  assert(Promoted_SignTrace_Get(0,&r) && r.yaw_mdeg==44);
  assert(r.heading_error_mdeg==-12000 && r.arc_peak_mdeg==160000 && r.left_cps==2200 && r.right_cps==0);
  assert(r.approach_from_pause==1);
  assert(r.exit_heading_peak_mdeg==55000);
  assert(r.road_reference_valid==1 && r.exit_reason==2 && r.arc_sweep_mdeg==130000);
  assert(r.line_action==3 && r.control_owner==6 && r.route_active==0);
  s.state=Promoted_SIGN_ROUTE_CANCELLED; s.fault=3;
  Promoted_SignTrace_Record(0xFFFFFF00U+299*20+1,0,&s,0,0,0,0,0); /* immediate transition, no 20ms wait */
  assert(Promoted_SignTrace_Get(255,&r) && r.fault==3);
  s.state=Promoted_SIGN_ROUTE_IDLE;
  for(i=0;i<100;++i) Promoted_SignTrace_Record(i*20,6,&s,0,0,0,1,0);
  assert(Promoted_SignTrace_Get(255,&r) && r.state==Promoted_SIGN_ROUTE_CANCELLED);
  Promoted_SignTrace_Request(0); Promoted_SignTrace_Task(0); assert(!writes);
  Promoted_SignTrace_Task(1); assert(writes==1);
  for(i=0;i<257;++i) Promoted_SignTrace_Task(1);
  assert(Promoted_SignTrace_Count()==256); /* dump and STOP preserve history */
  s.state=Promoted_SIGN_ROUTE_ARMED; s.fault=0;
  Promoted_SignTrace_Record(10000,6,&s,1412,1412,1,1,0);
  assert(Promoted_SignTrace_Get(255,&r) && r.state==Promoted_SIGN_ROUTE_ARMED && r.left_cps==1412);
  Promoted_SignTrace_Record(10001,0,&s,1870,1412,2,6,0);
  assert(Promoted_SignTrace_Get(255,&r) && r.mask==0 && r.control_owner==6 &&
         r.line_action==2 && r.left_cps==1870);
  Promoted_SignTrace_Request(1); Promoted_SignTrace_Task(0); assert(Promoted_SignTrace_Count()==256);
  Promoted_SignTrace_Task(1); assert(!Promoted_SignTrace_Count());
  puts("PASS: bounded trace, wrap, transition, freeze, STOP-only dump/clear, no motion-time UART");
  return 0;
}
