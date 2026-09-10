#include <assert.h>
#include <stdio.h>
#include "promoted_sign_observation.h"
#include "promoted_sign_route.h"

static void observation_pause(void)
{
  VisionDetection f = {0};
  Promoted_SignRouteStatus r;
  unsigned i;
  Promoted_SignObservation_Reset(); Promoted_SignRoute_Reset();
  Promoted_SignObservation_AllowPause(1);
  f.class_id=0; f.score=26; f.center_x=160; f.center_y=120;
  for(i=0;i<30;++i)
  {
    f.sequence=i+1; f.received_ms=100+i*100;
    Promoted_SignObservation_ObserveDetection(&f,f.received_ms);
    Promoted_SignRoute_ObserveDetection(&f);
    Promoted_SignRoute_GetStatus(f.received_ms,&r);
    Promoted_SignObservation_AllowPause(r.direction==0);
    assert(Promoted_SignObservation_Paused(f.received_ms)==(i<20));
  }
  Promoted_SignRoute_GetStatus(3000,&r); assert(r.direction==-1);
  /* Continuous/noisy detections cannot renew or rearm the deadline. */
  f.class_id=1; f.sequence++; f.received_ms=3100;
  Promoted_SignObservation_ObserveDetection(&f,3100); assert(!Promoted_SignObservation_Paused(3100));
  f.class_id=-1;
  for(i=0;i<=15;++i)
  { f.sequence++; f.received_ms=3200+i*100; Promoted_SignObservation_ObserveDetection(&f,f.received_ms); }
  f.class_id=1; f.sequence++; f.received_ms=4800;
  Promoted_SignObservation_ObserveDetection(&f,4800); assert(!Promoted_SignObservation_Paused(4800));
  Promoted_SignObservation_AllowPause(1); /* direction withdrawn/unconfirmed */
  f.sequence++;
  Promoted_SignObservation_ObserveDetection(&f,4800); assert(Promoted_SignObservation_Paused(4800));
  assert(Promoted_SignObservation_Paused(6799)); assert(!Promoted_SignObservation_Paused(6800));
  f.sequence++; f.received_ms=7299;
  Promoted_SignObservation_ObserveDetection(&f,7299); assert(!Promoted_SignObservation_Paused(7299));
  f.sequence++; f.received_ms=7300;
  Promoted_SignObservation_ObserveDetection(&f,7300); assert(Promoted_SignObservation_Paused(7300));
  assert(!Promoted_SignObservation_Paused(9300));
  Promoted_SignObservation_ObserveDetection(&f,9800); assert(!Promoted_SignObservation_Paused(9800)); /* duplicate */
  Promoted_SignObservation_Reset(); Promoted_SignObservation_AllowPause(1);
  f.sequence++; f.received_ms=10000; f.score=25;
  Promoted_SignObservation_ObserveDetection(&f,10000); assert(!Promoted_SignObservation_Paused(10000));
  f.sequence++; f.score=26;
  Promoted_SignObservation_ObserveDetection(&f,10000); assert(Promoted_SignObservation_Paused(10000));
  Promoted_SignObservation_Reset(); assert(!Promoted_SignObservation_Paused(4801));
  Promoted_SignObservation_AllowPause(0); f.sequence++; f.received_ms=5000;
  Promoted_SignObservation_ObserveDetection(&f,5000); assert(!Promoted_SignObservation_Paused(5000));
  Promoted_SignObservation_Reset(); Promoted_SignObservation_AllowPause(1);
  f.sequence++; f.received_ms=UINT32_MAX-999U;
  Promoted_SignObservation_ObserveDetection(&f,f.received_ms);
  assert(Promoted_SignObservation_Paused(999)); assert(!Promoted_SignObservation_Paused(1000));
  puts("PASS: 26-percent pause gate, fixed 2s, confirmed inhibition, 500ms retry, reset and wrap");
}

static void rejected_frames(void)
{
  VisionDetection f={0};
  unsigned i;
  for(i=0;i<7;++i)
  {
    Promoted_SignObservation_Reset(); Promoted_SignObservation_AllowPause(1);
    f.class_id=0; f.score=26; f.center_x=160; f.center_y=120;
    f.received_ms=100; f.sequence++;
    switch(i)
    {
      case 0: f.class_id=-1; break;
      case 1: f.class_id=2; break; /* horn must not trigger observation */
      case 2: f.class_id=3; break; /* digit */
      case 3: f.score=101; break;
      case 4: f.center_x=320; break;
      case 5: f.center_y=240; break;
      default: f.received_ms=0; break; /* stale */
    }
    Promoted_SignObservation_ObserveDetection(&f,400);
    assert(!Promoted_SignObservation_Paused(400));
  }
  Promoted_SignObservation_ObserveDetection(NULL,400);
  assert(!Promoted_SignObservation_Paused(400));
  puts("PASS: stale, non-arrow and malformed frames cannot trigger an observation stop");
}

static void centered_observation(void)
{
  VisionDetection f={0};
  unsigned i;
  uint32_t now=UINT32_MAX-999U, restarted;
  Promoted_SignObservation_Reset(); Promoted_SignObservation_AllowPause(1);
  f.class_id=0; f.score=26; f.center_x=160; f.center_y=120;
  f.sequence=1; f.received_ms=now;
  Promoted_SignObservation_ObserveDetection(&f,now);
  Promoted_SignObservation_UpdateLine(0,now);
  assert(Promoted_SignObservation_SeekingLine() && !Promoted_SignObservation_Paused(now));
  for(i=0;i<100;++i)
  {
    now+=40U; f.sequence++; f.received_ms=now;
    Promoted_SignObservation_ObserveDetection(&f,now);
    Promoted_SignObservation_UpdateLine(i%3==0?0:(i%3==1?2:4),now);
    assert(Promoted_SignObservation_SeekingLine() && !Promoted_SignObservation_Paused(now));
    assert(Promoted_SignObservation_HoldingRoute(now));
  }
  Promoted_SignObservation_AllowPause(0); /* confirmed direction cannot bypass restart */
  now+=10U; Promoted_SignObservation_UpdateLine(6,now);
  assert(Promoted_SignObservation_Paused(now) && Promoted_SignObservation_SeekingLine());
  now+=60U; Promoted_SignObservation_UpdateLine(6,now); /* gap restarts stability evidence */
  assert(Promoted_SignObservation_SeekingLine());
  for(i=0;i<3;++i) {now+=10U; Promoted_SignObservation_UpdateLine(6,now);}
  restarted=now;
  assert(!Promoted_SignObservation_SeekingLine() && Promoted_SignObservation_Paused(now));
  assert(Promoted_SignObservation_Paused(restarted+1999U));
  /* White at the exact deadline is checked BEFORE letting the pause end. */
  Promoted_SignObservation_UpdateLine(0,restarted+2000U);
  assert(Promoted_SignObservation_SeekingLine() && !Promoted_SignObservation_Paused(restarted+2000U));
  now=restarted+2010U;
  for(i=0;i<4;++i) {Promoted_SignObservation_UpdateLine(15,now);now+=10U;}
  restarted=now-10U;
  assert(!Promoted_SignObservation_SeekingLine()); /* both middle sensors also in 1111 */
  assert(Promoted_SignObservation_Paused(restarted+1999U));
  Promoted_SignObservation_UpdateLine(6,restarted+2000U);
  assert(!Promoted_SignObservation_HoldingRoute(restarted+2000U));
  Promoted_SignObservation_Reset();
  assert(!Promoted_SignObservation_SeekingLine() && !Promoted_SignObservation_HoldingRoute(now));
  puts("PASS: centering holds navigation, ignores partial contact/camera renewal, validates middle pair, restarts 2s and handles deadline loss/reset/wrap");
}
int main(void)
{
  centered_observation();
  observation_pause();
  rejected_frames();
  return 0;
}
