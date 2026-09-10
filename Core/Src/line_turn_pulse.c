#include "line_turn_pulse.h"
#define PULSE_PERIOD_MS 40U
#define PULSE_DRIVE_MS  LINE_TURN_PULSE_DRIVE_MS
#define PULSE_COAST_MS  16U
#define PULSE_PWM     3300

void LineTurnPulse_Update(LineTurnPulseState *s, uint32_t now,
                          const int32_t target[4], const int32_t count[4],
                          uint8_t degraded_mask, int16_t output[4])
{
  unsigned w;
  int32_t peak=0;
  uint8_t changed=0U;
  uint32_t elapsed;
  for(w=0;w<4;++w)
  {
    int32_t magnitude;
    int8_t direction=target[w]>0?1:(target[w]<0?-1:0);
    output[w]=0;
    if(target[w]<-9000 || target[w]>9000)
    { for(w=0;w<4;++w) output[w]=0; *s=(LineTurnPulseState){0}; return; }
    magnitude=target[w]<0?-target[w]:target[w];
    if(magnitude>peak) peak=magnitude;
    if(s->active && direction!=s->direction[w]) changed=1U;
  }
  if(!peak) { *s=(LineTurnPulseState){0}; return; }
  if(!s->active || changed)
  {
    /* Chattering direction commands may update the next push, but cannot
       keep postponing the same off phase forever. */
    if(!s->active || !s->reversing) s->started_ms=now;
    s->active=1U; s->reversing=changed; s->ended_mask=0U;
    for(w=0;w<4;++w)
    {
      s->direction[w]=target[w]>0?1:(target[w]<0?-1:0);
      s->start_count[w]=count[w];
    }
  }
  elapsed=now-s->started_ms;
  if(s->reversing)
  {
    if(elapsed<PULSE_COAST_MS) return;
    s->reversing=0U; s->started_ms=now; elapsed=0U;
    for(w=0;w<4;++w) s->start_count[w]=count[w];
  }
  if(elapsed>=PULSE_PERIOD_MS)
  {
    /* Never replay missed pulses after a delayed main loop. */
    s->started_ms=now; elapsed=0U; s->ended_mask=0U;
    for(w=0;w<4;++w) s->start_count[w]=count[w];
  }
  if(elapsed>=PULSE_DRIVE_MS) return;
  for(w=0;w<4;++w)
  {
    int32_t magnitude=target[w]<0?-target[w]:target[w];
    int32_t delta=(int32_t)((uint32_t)count[w]-(uint32_t)s->start_count[w]);
    int64_t forward=(int64_t)delta*s->direction[w];
    uint32_t width=((uint32_t)magnitude*PULSE_DRIVE_MS+(uint32_t)peak-1U)/(uint32_t)peak;
    uint32_t budget=((uint32_t)magnitude*PULSE_PERIOD_MS+999U)/1000U;
    if(elapsed>=width || (!(degraded_mask&(1U<<w)) && forward>=(int64_t)budget))
      s->ended_mask|=(uint8_t)(1U<<w);
    if(magnitude && elapsed<width && !(s->ended_mask&(1U<<w)))
      output[w]=(int16_t)(s->direction[w]*PULSE_PWM);
  }
}
