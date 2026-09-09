#include "sign_horn.h"
#include <stddef.h>
static uint32_t sequence, last_ms, absent_ms;
static uint8_t valid, votes, latched, absent;
void SignHorn_Reset(void)
{ sequence=last_ms=absent_ms=0U; valid=votes=latched=absent=0U; }
uint8_t SignHorn_Observe(const VisionDetection *f, uint32_t now)
{
  if (!f || (valid && f->sequence==sequence)) return 0;
  if (now-f->received_ms>350U || f->class_id < -1 || f->class_id>4 ||
      f->score>100 || f->center_x>=320 || f->center_y>=240)
  { votes=absent=0; return 0; }
  if (valid && (f->sequence!=sequence+1U || f->received_ms-last_ms>300U))
    votes=absent=0;
  sequence=f->sequence; last_ms=f->received_ms; valid=1;
  if(f->class_id==2)
  {
    absent=0;
    if(f->score<20) { votes=0; return 0; }
    if(latched) return 0;
    if(++votes>=3) { latched=1; votes=0; return 1; }
  }
  else
  {
    votes=0;
    if(!absent) { absent=1; absent_ms=f->received_ms; }
    if(f->received_ms-absent_ms>=800U) latched=0;
  }
  return 0;
}
