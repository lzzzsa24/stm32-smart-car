#include "line_fault_log.h"
#include "diagnostic_uart.h"

static LineFaultRecord records[LINE_FAULT_LOG_CAPACITY];
static uint32_t count, next, sequence, overwritten;
static uint8_t dump_pending, dump_active, dump_part;
static uint32_t dump_index;
static LineSearchRecord searches[LINE_SEARCH_LOG_CAPACITY];
static uint32_t search_count, search_next, search_overwritten;

void LineFaultLog_Init(void)
{
  count = next = sequence = overwritten = 0U;
  dump_pending = dump_active = dump_part = 0U;
  dump_index = 0U;
  search_count = search_next = search_overwritten = 0U;
}
uint32_t LineFaultLog_SearchCount(void) { return search_count; }
void LineFaultLog_RecordSearch(const LineSearchRecord *r)
{
  if (!r) return;
  searches[search_next] = *r;
  search_next = (search_next + 1U) % LINE_SEARCH_LOG_CAPACITY;
  if (search_count < LINE_SEARCH_LOG_CAPACITY) ++search_count;
  else if (search_overwritten != UINT32_MAX) ++search_overwritten;
}
uint8_t LineFaultLog_GetSearch(uint32_t index, LineSearchRecord *r)
{
  if (!r || index >= search_count) return 0U;
  *r = searches[(search_next + LINE_SEARCH_LOG_CAPACITY - search_count + index) % LINE_SEARCH_LOG_CAPACITY];
  return 1U;
}
uint32_t LineFaultLog_Count(void) { return count; }
uint32_t LineFaultLog_Overwritten(void) { return overwritten; }
uint8_t LineFaultLog_Get(uint32_t index, LineFaultRecord *r)
{
  if (!r || index >= count) return 0U;
  *r = records[(next + LINE_FAULT_LOG_CAPACITY - count + index) % LINE_FAULT_LOG_CAPACITY];
  return 1U;
}
void LineFaultLog_Record(const LineFaultRecord *r)
{
  if (!r) return;
  if (count)
  {
    LineFaultRecord *last = &records[(next + LINE_FAULT_LOG_CAPACITY - 1U) % LINE_FAULT_LOG_CAPACITY];
    /* Keep the first snapshot of an ongoing incident; update its duration.
       A gap of one second or a different fault combination starts a record. */
    if (r->first_ms - last->last_ms < 1000U &&
        r->stall_mask == last->stall_mask && r->direction_mask == last->direction_mask &&
        r->signal_mask == last->signal_mask)
    {
      last->last_ms = r->first_ms;
      if (last->occurrences != UINT32_MAX) ++last->occurrences;
      return;
    }
  }
  records[next] = *r;
  records[next].sequence = ++sequence;
  records[next].last_ms = r->first_ms;
  records[next].occurrences = 1U;
  next = (next + 1U) % LINE_FAULT_LOG_CAPACITY;
  if (count < LINE_FAULT_LOG_CAPACITY) ++count;
  else if (overwritten != UINT32_MAX) ++overwritten;
}
static void u(const char *key, uint32_t value)
{ DiagnosticUart_WriteString(key); DiagnosticUart_WriteUnsigned(value); }
static void s(const char *key, int32_t value)
{ DiagnosticUart_WriteString(key); DiagnosticUart_WriteSigned(value); }
void LineFaultLog_RequestDump(void)
{ dump_pending = 1U; dump_active = 0U; }
void LineFaultLog_Task(uint8_t stopped)
{
  LineFaultRecord r;
  uint8_t w;
  if (!stopped)
  {
    if (dump_active) dump_pending = 1U;
    dump_active = 0U;
    return;
  }
  if (dump_pending)
  {
    dump_pending = 0U; dump_active = 1U; dump_index = 0U; dump_part = 0U;
    DiagnosticUart_WriteString("LFAULT BEGIN v=1 storage=RAM policy=LINE_OBSERVE");
    u(" count=", count); u(" overwritten=", overwritten);
    DiagnosticUart_WriteString("\r\n");
    return;
  }
  if (!dump_active) return;
  if (dump_active == 2U)
  {
    DiagnosticUart_WriteString("LSEARCH BEGIN v=1 sources=0:default,1:hint,2:rejoin,3:corner,4:correction,5:wait,6:cross_hint");
    u(" count=", search_count); u(" overwritten=", search_overwritten);
    DiagnosticUart_WriteString("\r\n"); dump_index = 0U; dump_active = 3U; return;
  }
  if (dump_active == 3U)
  {
    LineSearchRecord decision;
    if (!LineFaultLog_GetSearch(dump_index, &decision))
    { DiagnosticUart_WriteString("LSEARCH END\r\n"); dump_active = 0U; return; }
    DiagnosticUart_WriteString("LSEARCH"); u(" ms=", decision.time_ms);
    s(" side=", decision.chosen_side); u(" source=", (uint32_t)decision.source);
    s(" hint=", decision.hint); u(" edge=", decision.edge_mask); u(" edge_age=", decision.edge_age_ms);
    u(" wide=", decision.wide_mask); u(" wide_age=", decision.wide_age_ms);
    u(" queue_overwritten=", decision.queue_overwritten);
    u(" pause=", decision.pause_reason); u(" drive_fault=", decision.drive_fault);
    u(" bypass_fault=", decision.bypass_fault);
    u(" hint_mask=", decision.hint_mask); u(" hint_age=", decision.hint_age_ms);
    DiagnosticUart_WriteString("\r\n"); ++dump_index; return;
  }
  if (!LineFaultLog_Get(dump_index, &r))
  {
    DiagnosticUart_WriteString("LFAULT END\r\n"); dump_active = 2U; return;
  }
  DiagnosticUart_WriteString("LFAULT"); u(" seq=", r.sequence);
  if (!dump_part)
  {
    u(" first_ms=", r.first_ms); u(" last_ms=", r.last_ms); u(" hits=", r.occurrences);
    u(" dt=", r.elapsed_ms); u(" stall=", r.stall_mask); u(" direction=", r.direction_mask);
    u(" signal=", r.signal_mask); u(" sensors=", r.sensor_mask);
    u(" recovery=", r.recovery_state); u(" degraded=", r.degraded_mask); u(" mv=", r.battery_mv);
  }
  else
  {
    w = (uint8_t)(dump_part - 1U); u(" wheel=", (uint32_t)w + 1U);
    s(" requested=", r.requested[w]); s(" controlled=", r.controlled[w]);
    s(" measured=", r.measured[w]); s(" pwm=", r.pwm[w]); s(" delta=", r.delta[w]);
    u(" illegal=", r.illegal_delta[w]); u(" zero_ms=", r.no_motion_ms[w]);
  }
  DiagnosticUart_WriteString("\r\n");
  if (++dump_part == 5U) { dump_part = 0U; ++dump_index; }
}
