#include <assert.h>
#include <stdio.h>
#include "promoted_sign_horn.h"
static VisionDetection f;
static unsigned now;
static unsigned send(int cls, unsigned dt)
{
  now+=dt; ++f.sequence; f.received_ms=now; f.class_id=(int8_t)cls;
  f.score=80; f.center_x=160; f.center_y=120;
  return Promoted_SignHorn_Observe(&f,now);
}
int main(void)
{
  unsigned i;
  now=0xFFFFFF00U; f.sequence=0xFFFFFFFEU;
  Promoted_SignHorn_Reset(); assert(!send(2,100)); assert(!send(2,100)); assert(send(2,100));
  for(i=0;i<100;++i) assert(!send(2,100));
  assert(!Promoted_SignHorn_Observe(&f,now)); /* duplicate */
  assert(!send(-1,100)); assert(!send(-1,1000)); /* silence isn't absence */
  assert(!send(2,100));
  for(i=0;i<9;++i) assert(!send(-1,100));
  assert(!send(2,100)); assert(!send(2,100)); assert(send(2,100));
  Promoted_SignHorn_Reset(); assert(!send(2,100)); assert(!send(0,100));
  assert(!send(2,100)); assert(!send(2,100)); assert(send(2,100));
  Promoted_SignHorn_Reset(); assert(!send(2,100));
  f.sequence+=2; assert(!send(2,100)); assert(!send(2,100)); assert(send(2,100));
  Promoted_SignHorn_Reset(); assert(!send(2,100));
  ++f.sequence; assert(!Promoted_SignHorn_Observe(&f,now+1000));
  assert(!send(2,100)); assert(!send(2,100)); assert(send(2,100));
  puts("PASS: horn confirmation, continuous hold, fresh absence rearm, reset, sequence and clock wrap");
  return 0;
}
