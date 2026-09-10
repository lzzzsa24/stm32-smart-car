#include "sign_trace.h"
#include "diagnostic_uart.h"
#include <string.h>
static SignTraceRecord records[SIGN_TRACE_CAPACITY];
static uint16_t head, count, cursor;
static uint8_t frozen, request, dumping;
void SignTrace_Init(void)
{ head=count=cursor=0; frozen=request=dumping=0; }
uint16_t SignTrace_Count(void) { return count; }
uint8_t SignTrace_Get(uint16_t index, SignTraceRecord *out)
{
  if (!out || index>=count) return 0;
  *out=records[(head+SIGN_TRACE_CAPACITY-count+index)%SIGN_TRACE_CAPACITY];
  return 1;
}
void SignTrace_Record(uint32_t now, uint8_t mask, const SignRouteStatus *s,
                      int32_t left_cps, int32_t right_cps, uint8_t line_action,
                      uint8_t control_owner, uint8_t route_active)
{
  SignTraceRecord r={0}, last;
  if (!s) return;
  if (frozen && (s->state==SIGN_ROUTE_ARMED || s->state==SIGN_ROUTE_PROBE ||
                 s->state==SIGN_ROUTE_SELECTING)) frozen=0;
  if (frozen) return;
  dumping=0; /* starting motion cancels an incomplete dump */
  if (count && SignTrace_Get((uint16_t)(count-1),&last) &&
      now-last.time_ms<20U && last.state==(uint8_t)s->state && last.fault==s->fault &&
      last.mask==mask && last.left_cps==left_cps && last.right_cps==right_cps &&
      last.line_action==line_action && last.control_owner==control_owner &&
      last.route_active==route_active)
    return;
  r.time_ms=now; r.sequence=s->last_sequence; r.yaw_mdeg=s->yaw_mdeg;
  r.travel_mm=s->travel_mm; r.mask=mask; r.state=(uint8_t)s->state;
  r.fault=s->fault; r.online=s->vision_online; r.score=s->last_score;
  r.direction=s->direction; r.class_id=s->last_class;
  r.heading_error_mdeg=s->heading_error_mdeg; r.arc_peak_mdeg=s->arc_peak_mdeg;
  r.left_cps=left_cps; r.right_cps=right_cps;
  r.approach_from_pause=s->approach_from_pause;
  r.exit_heading_peak_mdeg=s->exit_heading_peak_mdeg;
  r.road_reference_valid=s->road_reference_valid;
  r.exit_reason=s->exit_reason; r.arc_sweep_mdeg=s->arc_sweep_mdeg;
  r.line_action=line_action; r.control_owner=control_owner;
  r.route_active=route_active;
  records[head]=r; head=(uint16_t)((head+1)%SIGN_TRACE_CAPACITY);
  if(count<SIGN_TRACE_CAPACITY) ++count;
  if(s->state==SIGN_ROUTE_CANCELLED) frozen=1;
}
void SignTrace_Request(uint8_t clear) { request=clear?2U:1U; }
void SignTrace_Task(uint8_t stopped)
{
  SignTraceRecord r;
  if(!stopped) { dumping=0; return; }
  if(request==2U) { SignTrace_Init(); DiagnosticUart_WriteString("STRACE CLEARED\r\n"); return; }
  if(request==1U)
  {
    request=0; cursor=0; dumping=1;
    DiagnosticUart_WriteString("STRACE BEGIN t,mask,state,dir,fault,yaw,mm,seq,online,class,score,heading_error,arc_peak,left_cps,right_cps,pause_ref,exit_heading_peak,road_ref_valid,exit_reason,arc_sweep,line_action,owner,route_active\r\n");
    return;
  }
  if(!dumping) return;
  if(!SignTrace_Get(cursor++,&r))
  { dumping=0; DiagnosticUart_WriteString("STRACE END\r\n"); return; }
  DiagnosticUart_WriteString("STRACE ");
  DiagnosticUart_WriteUnsigned(r.time_ms);
#define FIELD(value) do { DiagnosticUart_WriteString(","); DiagnosticUart_WriteSigned(value); } while(0)
  FIELD(r.mask); FIELD(r.state); FIELD(r.direction); FIELD(r.fault);
  FIELD(r.yaw_mdeg); FIELD(r.travel_mm);
  DiagnosticUart_WriteString(","); DiagnosticUart_WriteUnsigned(r.sequence);
  FIELD(r.online); FIELD(r.class_id); FIELD(r.score);
  FIELD(r.heading_error_mdeg); FIELD(r.arc_peak_mdeg); FIELD(r.left_cps); FIELD(r.right_cps);
  FIELD(r.approach_from_pause);
  FIELD(r.exit_heading_peak_mdeg);
  FIELD(r.road_reference_valid); FIELD(r.exit_reason); FIELD(r.arc_sweep_mdeg);
  FIELD(r.line_action); FIELD(r.control_owner); FIELD(r.route_active);
#undef FIELD
  DiagnosticUart_WriteString("\r\n");
}
