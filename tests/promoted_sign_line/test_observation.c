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
  f.class_id=0; f.score=20; f.center_x=160; f.center_y=120;
  for(i=0;i<30;++i)
  {
    f.sequence=i+1; f.received_ms=100+i*100;
    Promoted_SignObservation_ObserveDetection(&f,f.received_ms,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM);
    Promoted_SignRoute_ObserveDetection(&f);
    Promoted_SignRoute_GetStatus(f.received_ms,&r);
    Promoted_SignObservation_AllowPause(r.direction==0);
    assert(Promoted_SignObservation_Paused(f.received_ms)==(i<20));
  }
  Promoted_SignRoute_GetStatus(3000,&r); assert(r.direction==-1);
  /* Continuous/noisy detections cannot renew or rearm the deadline. */
  f.class_id=1; f.sequence++; f.received_ms=3100;
  Promoted_SignObservation_ObserveDetection(&f,3100,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM); assert(!Promoted_SignObservation_Paused(3100));
  f.class_id=-1;
  for(i=0;i<=15;++i)
  { f.sequence++; f.received_ms=3200+i*100; Promoted_SignObservation_ObserveDetection(&f,f.received_ms,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM); }
  f.class_id=1; f.sequence++; f.received_ms=4800;
  Promoted_SignObservation_ObserveDetection(&f,4800,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM); assert(!Promoted_SignObservation_Paused(4800));
  Promoted_SignObservation_AllowPause(1); /* direction withdrawn/unconfirmed */
  f.sequence++;
  Promoted_SignObservation_ObserveDetection(&f,4800,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM); assert(Promoted_SignObservation_Paused(4800));
  assert(Promoted_SignObservation_Paused(6799)); assert(!Promoted_SignObservation_Paused(6800));
  f.sequence++; f.received_ms=7299;
  Promoted_SignObservation_ObserveDetection(&f,7299,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM); assert(!Promoted_SignObservation_Paused(7299));
  f.sequence++; f.received_ms=7300;
  Promoted_SignObservation_ObserveDetection(&f,7300,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM); assert(Promoted_SignObservation_Paused(7300));
  assert(!Promoted_SignObservation_Paused(9300));
  Promoted_SignObservation_ObserveDetection(&f,9800,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM); assert(!Promoted_SignObservation_Paused(9800)); /* duplicate */
  Promoted_SignObservation_Reset(); Promoted_SignObservation_AllowPause(1);
  f.sequence++; f.received_ms=10000; f.score=19;
  Promoted_SignObservation_ObserveDetection(&f,10000,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM); assert(!Promoted_SignObservation_Paused(10000));
  f.sequence++; f.score=20;
  Promoted_SignObservation_ObserveDetection(&f,10000,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM); assert(Promoted_SignObservation_Paused(10000));
  Promoted_SignObservation_Reset(); assert(!Promoted_SignObservation_Paused(4801));
  Promoted_SignObservation_AllowPause(0); f.sequence++; f.received_ms=5000;
  Promoted_SignObservation_ObserveDetection(&f,5000,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM); assert(!Promoted_SignObservation_Paused(5000));
  Promoted_SignObservation_Reset(); Promoted_SignObservation_AllowPause(1);
  f.sequence++; f.received_ms=UINT32_MAX-999U;
  Promoted_SignObservation_ObserveDetection(&f,f.received_ms,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM);
  assert(Promoted_SignObservation_Paused(999)); assert(!Promoted_SignObservation_Paused(1000));
  puts("PASS: 20-percent pause gate, fixed 2s, confirmed inhibition, 500ms retry, reset and wrap");
}

static void rejected_frames(void)
{
  VisionDetection f={0};
  unsigned i;
  for(i=0;i<7;++i)
  {
    Promoted_SignObservation_Reset(); Promoted_SignObservation_AllowPause(1);
    f.class_id=0; f.score=22; f.center_x=160; f.center_y=120;
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
    Promoted_SignObservation_ObserveDetection(&f,400,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM);
    assert(!Promoted_SignObservation_Paused(400));
  }
  Promoted_SignObservation_ObserveDetection(NULL,400,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM);
  assert(!Promoted_SignObservation_Paused(400));
  puts("PASS: stale, non-arrow and malformed frames cannot trigger an observation stop");
}

/* Fixed pause timing is independent of sensors; motor-path masks are covered
   by test_mode2_follow.c. */
static void threshold_layers(void)
{
  VisionDetection f={0};
  Promoted_SignRouteStatus r;
  unsigned score,i;
  int side;
  for(side=0;side<=1;++side) for(score=19;score<=26;++score)
  {
    Promoted_SignObservation_Reset(); Promoted_SignRoute_Reset();
    Promoted_SignRoute_SetProfile(Promoted_SIGN_ROUTE_PROFILE_STANDARD);
    f.class_id=(int8_t)side; f.score=(uint8_t)score;
    f.center_x=160; f.center_y=120;
    for(i=0;i<3;++i)
    {
      f.sequence=i+1; f.received_ms=100U+i*100U;
      Promoted_SignRoute_GetStatus(f.received_ms,&r);
      Promoted_SignObservation_AllowPause(r.direction==0);
      Promoted_SignObservation_ObserveDetection(&f,f.received_ms,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM);
      Promoted_SignRoute_ObserveDetection(&f);
      assert(Promoted_SignObservation_Paused(f.received_ms)==(score>=20));
    }
    Promoted_SignRoute_GetStatus(f.received_ms,&r);
    assert(r.direction==(score>=20 ? (side==0?-1:1) : 0));
    if(score==19)
    {
      /* Weak votes remain provisional until the first parking-grade frame. */
      Promoted_SignObservation_AllowPause(r.direction==0);
      for(i=0;i<3;++i)
      {
        f.sequence++; f.received_ms+=100; f.score=20;
        Promoted_SignObservation_ObserveDetection(&f,f.received_ms,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM);
        Promoted_SignRoute_ObserveDetection(&f);
        assert(Promoted_SignObservation_Paused(f.received_ms));
        Promoted_SignRoute_GetStatus(f.received_ms,&r);
        assert(r.direction==(i==2 ? (side==0?-1:1) : 0));
      }
    }
    Promoted_SignObservation_Reset(); Promoted_SignObservation_AllowPause(1);
    f.score=(uint8_t)score; f.sequence++;
    Promoted_SignObservation_ObserveDetection(&f,f.received_ms,Promoted_SIGN_OBSERVATION_MODE4_SCORE_MINIMUM);
    assert(Promoted_SignObservation_Paused(f.received_ms)==(score>=26));
  }
  puts("PASS: mode3 19 rejected/20 accepted before three-vote direction; mode4 retains 26-percent parking");
}

static void confirmation_gate_window(void)
{
  VisionDetection f={0};
  Promoted_SignRouteStatus r;
  unsigned profile,scenario,i;
  for(profile=0;profile<2;++profile) for(scenario=0;scenario<5;++scenario)
  {
    Promoted_SignRoute_Reset(); Promoted_SignRoute_SetProfile((Promoted_SignRouteProfile)profile);
    f.class_id=0; f.score=22; f.center_x=160; f.center_y=120;
    f.sequence=1; f.received_ms=100;
    Promoted_SignRoute_ObserveDetection(&f);
    if(scenario==1) f.received_ms+=700U; /* qualifying frame expires before votes */
    for(i=0;i<6;++i)
    {
      f.sequence++;
      f.received_ms+=100U;
      f.class_id=scenario==0?1:0;
      f.score=19;
      if(scenario==2 && i==0) f.sequence++; /* queue gap */
      if(scenario==3) f.center_x=250;       /* new object */
      if(scenario==4 && i<3) f.class_id=-1; /* evict qualifying vote before a majority */
      Promoted_SignRoute_ObserveDetection(&f);
    }
    Promoted_SignRoute_GetStatus(f.received_ms,&r);
    assert(r.direction==0);
    for(i=0;i<3;++i)
    {
      f.sequence++; f.received_ms+=100; f.score=20;
      Promoted_SignRoute_ObserveDetection(&f);
      Promoted_SignRoute_GetStatus(f.received_ms,&r);
      assert(r.direction==(i==2 ? (scenario==0?1:-1) : 0));
    }
  }
  Promoted_SignRoute_SetProfile(Promoted_SIGN_ROUTE_PROFILE_STANDARD);
  puts("PASS: observation-grade vote is same-class, fresh and cleared by queue gap, object jump or window eviction; mode4 voting unchanged");
}
static void seek_and_restart(void)
{
  VisionDetection f={0};
  uint32_t start=UINT32_MAX-500U, found=start+4000U;
  unsigned middle;
  for(middle=2;middle<=4;middle+=2)
  {
    Promoted_SignObservation_Reset(); Promoted_SignObservation_AllowPause(1);
    f.class_id=0; f.score=22; f.center_x=160; f.center_y=120;
    f.sequence=1; f.received_ms=start;
    Promoted_SignObservation_ObserveDetection(&f,start,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM);
    Promoted_SignObservation_UpdateLine(0,start);
    assert(Promoted_SignObservation_SeekingLine() && !Promoted_SignObservation_Paused(start));
    f.sequence++; f.received_ms=found;
    Promoted_SignObservation_ObserveDetection(&f,found,Promoted_SIGN_OBSERVATION_MODE3_SCORE_MINIMUM);
    Promoted_SignObservation_UpdateLine(9,found);
    assert(Promoted_SignObservation_SeekingLine() && !Promoted_SignObservation_Paused(found));
    Promoted_SignObservation_AllowPause(0); /* confirmation does not bypass finding line */
    Promoted_SignObservation_UpdateLine((uint8_t)middle,found);
    assert(!Promoted_SignObservation_SeekingLine() && Promoted_SignObservation_Paused(found+1999U));
    Promoted_SignObservation_UpdateLine(0,found+2000U); /* exact-deadline loss seeks again */
    assert(Promoted_SignObservation_SeekingLine() && !Promoted_SignObservation_Paused(found+2000U));
    Promoted_SignObservation_UpdateLine((uint8_t)middle,found+2010U);
    assert(Promoted_SignObservation_Paused(found+4009U));
    Promoted_SignObservation_UpdateLine((uint8_t)middle,found+4010U);
    assert(!Promoted_SignObservation_HoldingRoute(found+4010U));
    Promoted_SignObservation_Reset(); assert(!Promoted_SignObservation_SeekingLine());
  }
  puts("PASS: either middle captures immediately; outer-only and renewed frames cannot end seeking; deadline white restarts, reset and wrap");
}
int main(void)
{
  confirmation_gate_window();
  seek_and_restart();
  threshold_layers();
  observation_pause();
  rejected_frames();
  return 0;
}
