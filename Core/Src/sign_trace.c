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
void SignTrace_Record(uint32_t now, uint8_t mask, const SignRouteStatus *s)
{
  SignTraceRecord r={0}, last;
  if (!s || frozen) return;
  dumping=0; /* starting motion cancels an incomplete dump */
  if (count && SignTrace_Get((uint16_t)(count-1),&last) &&
      now-last.time_ms<20U && last.state==(uint8_t)s->state && last.fault==s->fault)
    return;
  r.time_ms=now; r.sequence=s->last_sequence; r.yaw_mdeg=s->yaw_mdeg;
  r.travel_mm=s->travel_mm; r.mask=mask; r.state=(uint8_t)s->state;
  r.fault=s->fault; r.online=s->vision_online; r.score=s->last_score;
  r.direction=s->direction; r.class_id=s->last_class;
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
    DiagnosticUart_WriteString("STRACE BEGIN t,mask,state,dir,fault,yaw,mm,seq,online,class,score\r\n");
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
#undef FIELD
  DiagnosticUart_WriteString("\r\n");
}
