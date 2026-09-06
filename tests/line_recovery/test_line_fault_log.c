#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "line_fault_log.h"
#include "diagnostic_uart.h"
static char text[40000];
static size_t used;
void DiagnosticUart_WriteString(const char *s)
{
  size_t n=strlen(s); assert(used+n<sizeof text);
  memcpy(text+used,s,n+1); used+=n;
}
void DiagnosticUart_WriteUnsigned(uint32_t v)
{ char b[32]; snprintf(b,sizeof b,"%lu",(unsigned long)v); DiagnosticUart_WriteString(b); }
void DiagnosticUart_WriteSigned(int32_t v)
{ char b[32]; snprintf(b,sizeof b,"%ld",(long)v); DiagnosticUart_WriteString(b); }
int main(void)
{
  LineFaultRecord r={0}, out={0}; unsigned i;
  LineFaultLog_Init(); assert(!LineFaultLog_Count());
  {
    LineSearchRecord decision={0}, saved={0};
    decision.chosen_side=1; decision.hint=1; decision.source=LINE_SEARCH_HINT;
    decision.edge_mask=8; decision.edge_age_ms=100; decision.wide_mask=7; decision.wide_age_ms=101;
    for(i=0;i<18;++i) { decision.time_ms=i; LineFaultLog_RecordSearch(&decision); }
    assert(LineFaultLog_SearchCount()==16 && LineFaultLog_GetSearch(0,&saved) && saved.time_ms==2);
    assert(!LineFaultLog_GetSearch(16,&saved) && !LineFaultLog_GetSearch(0,0));
    used=0; text[0]=0; LineFaultLog_RequestDump();
    for(i=0;i<30;++i) LineFaultLog_Task(0);
    assert(!used);
    for(i=0;i<25;++i) LineFaultLog_Task(1);
    assert(strstr(text,"LFAULT END\r\nLSEARCH BEGIN") && strstr(text,"count=16 overwritten=2"));
    assert(strstr(text,"side=1 source=1 hint=1 edge=8 edge_age=100 wide=7 wide_age=101"));
    assert(strstr(text,"LSEARCH END\r\n") && LineFaultLog_SearchCount()==16);
    LineFaultLog_RequestDump();
    for(i=0;i<5;++i) LineFaultLog_Task(1);
    LineFaultLog_Task(0); used=0; text[0]=0;
    for(i=0;i<25;++i) LineFaultLog_Task(1);
    assert(strstr(text,"LSEARCH BEGIN") && strstr(text,"LSEARCH END"));
    LineFaultLog_Init(); assert(!LineFaultLog_SearchCount());
    used=0; text[0]=0;
  }
  assert(!LineFaultLog_Get(0,&out));
  r.first_ms=100; r.direction_mask=4; r.sensor_mask=5; r.pwm[2]=-2600;
  r.delta[2]=1; r.controlled[2]=-2493; r.battery_mv=7800;
  LineFaultLog_Record(&r); r.first_ms=120; r.pwm[2]=-2800; LineFaultLog_Record(&r);
  assert(LineFaultLog_Count()==1 && LineFaultLog_Get(0,&out));
  assert(out.pwm[2]==-2600 && out.first_ms==100 && out.last_ms==120 && out.occurrences==2);
  LineFaultLog_RequestDump();
  for(i=0;i<20;++i) LineFaultLog_Task(0);
  assert(!used); /* Request while moving is deferred, never blocks control. */
  for(i=0;i<10;++i) LineFaultLog_Task(1);
  assert(strstr(text,"LFAULT BEGIN v=1 storage=RAM") && strstr(text,"direction=4"));
  assert(strstr(text,"wheel=3 requested=0 controlled=-2493 measured=0 pwm=-2600 delta=1"));
  assert(strstr(text,"LFAULT END\r\n"));
  /* Reading is non-destructive and reissuing f starts a complete dump. */
  used=0; text[0]=0; LineFaultLog_RequestDump(); LineFaultLog_Task(1);
  LineFaultLog_Task(0); used=0; text[0]=0;
  for(i=0;i<10;++i) LineFaultLog_Task(1);
  assert(strstr(text,"LFAULT BEGIN") && strstr(text,"LFAULT END") && LineFaultLog_Count()==1);
  for(i=0;i<40;++i)
  { r.first_ms=2000+i*1000; LineFaultLog_Record(&r); }
  assert(LineFaultLog_Count()==32 && LineFaultLog_Overwritten()==9);
  assert(LineFaultLog_Get(0,&out) && out.sequence==10 && out.first_ms==10000);
  assert(LineFaultLog_Get(31,&out) && out.sequence==41 && out.first_ms==41000);
  assert(!LineFaultLog_Get(32,&out) && !LineFaultLog_Get(0,0));
  LineFaultLog_Init(); r.first_ms=UINT32_MAX-10; LineFaultLog_Record(&r);
  r.first_ms=9; LineFaultLog_Record(&r);
  assert(LineFaultLog_Count()==1 && LineFaultLog_Get(0,&out) && out.occurrences==2 && out.last_ms==9);
  LineFaultLog_Init(); assert(!LineFaultLog_Count());
  puts("PASS: RAM fault history, coalescing, ring overwrite, wrap, deferred/restartable serial dump");
  {
    LineSearchRecord waited={0};
    waited.source=LINE_SEARCH_WAIT_RECOVERY; waited.chosen_side=-1;
    waited.pause_reason=1; waited.drive_fault=32; waited.bypass_fault=64;
    LineFaultLog_RecordSearch(&waited);
    used=0; text[0]=0; LineFaultLog_RequestDump();
    for(i=0;i<10;++i) LineFaultLog_Task(1);
    assert(strstr(text,"source=5") && strstr(text,"pause=1 drive_fault=32 bypass_fault=64"));
    assert(strstr(text,"LSEARCH END"));
  }
  return 0;
}
