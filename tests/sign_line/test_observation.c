#include <assert.h>
#include <stdio.h>
#include "sign_observation.h"
#include "sign_route.h"

static void observation_pause(void)
{
  VisionDetection f = {0};
  SignRouteStatus r;
  unsigned i;
  SignObservation_Reset(); SignRoute_Reset();
  SignObservation_AllowPause(1);
  f.class_id=0; f.score=25; f.center_x=160; f.center_y=120;
  for(i=0;i<30;++i)
  {
    f.sequence=i+1; f.received_ms=100+i*100;
    SignObservation_ObserveDetection(&f,f.received_ms);
    SignRoute_ObserveDetection(&f);
    SignRoute_GetStatus(f.received_ms,&r);
    SignObservation_AllowPause(r.direction==0);
    assert(SignObservation_Paused(f.received_ms)==(i<20));
  }
  SignRoute_GetStatus(3000,&r); assert(r.direction==-1);
  /* Continuous/noisy detections cannot renew or rearm the deadline. */
  f.class_id=1; f.sequence++; f.received_ms=3100;
  SignObservation_ObserveDetection(&f,3100); assert(!SignObservation_Paused(3100));
  f.class_id=-1;
  for(i=0;i<=15;++i)
  { f.sequence++; f.received_ms=3200+i*100; SignObservation_ObserveDetection(&f,f.received_ms); }
  f.class_id=1; f.sequence++; f.received_ms=4800;
  SignObservation_ObserveDetection(&f,4800); assert(!SignObservation_Paused(4800));
  SignObservation_AllowPause(1); /* direction withdrawn/unconfirmed */
  f.sequence++;
  SignObservation_ObserveDetection(&f,4800); assert(SignObservation_Paused(4800));
  assert(SignObservation_Paused(6799)); assert(!SignObservation_Paused(6800));
  f.sequence++; f.received_ms=7299;
  SignObservation_ObserveDetection(&f,7299); assert(!SignObservation_Paused(7299));
  f.sequence++; f.received_ms=7300;
  SignObservation_ObserveDetection(&f,7300); assert(SignObservation_Paused(7300));
  assert(!SignObservation_Paused(9300));
  SignObservation_ObserveDetection(&f,9800); assert(!SignObservation_Paused(9800)); /* duplicate */
  SignObservation_Reset(); SignObservation_AllowPause(1);
  f.sequence++; f.received_ms=10000; f.score=24;
  SignObservation_ObserveDetection(&f,10000); assert(!SignObservation_Paused(10000));
  f.sequence++; f.score=25;
  SignObservation_ObserveDetection(&f,10000); assert(SignObservation_Paused(10000));
  SignObservation_Reset(); assert(!SignObservation_Paused(4801));
  SignObservation_AllowPause(0); f.sequence++; f.received_ms=5000;
  SignObservation_ObserveDetection(&f,5000); assert(!SignObservation_Paused(5000));
  SignObservation_Reset(); SignObservation_AllowPause(1);
  f.sequence++; f.received_ms=UINT32_MAX-999U;
  SignObservation_ObserveDetection(&f,f.received_ms);
  assert(SignObservation_Paused(999)); assert(!SignObservation_Paused(1000));
  puts("PASS: 25-percent pause gate, fixed 2s, confirmed inhibition, 500ms retry, reset and wrap");
}

static void rejected_frames(void)
{
  VisionDetection f={0};
  unsigned i;
  for(i=0;i<7;++i)
  {
    SignObservation_Reset(); SignObservation_AllowPause(1);
    f.class_id=0; f.score=25; f.center_x=160; f.center_y=120;
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
    SignObservation_ObserveDetection(&f,400);
    assert(!SignObservation_Paused(400));
  }
  SignObservation_ObserveDetection(NULL,400);
  assert(!SignObservation_Paused(400));
  puts("PASS: stale, non-arrow and malformed frames cannot trigger an observation stop");
}

int main(void)
{
  observation_pause();
  rejected_frames();
  return 0;
}
