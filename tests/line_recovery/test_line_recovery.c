/* Real line + recovery + frozen phrase. HAL and wheel motion are simulated. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "line_tracking.h"
#include "line_recovery.h"
#include "line_search_model.h"
#include "line_sensor_sample.h"
#include "line_fault_log.h"
#include "diagnostic_uart.h"
void DiagnosticUart_WriteString(const char *s) { (void)s; }
void DiagnosticUart_WriteUnsigned(uint32_t v) { (void)v; }
void DiagnosticUart_WriteSigned(int32_t v) { (void)v; }
#include "drive_base.h"
#include "buzzer_phrase_40077493715.h"
static uint32_t tick, brake_started;
static DriveBaseTelemetry telemetry;
static LineTrackingCommand output;
static GPIO_PinState buzzer;
static unsigned attacks, spins, reversals, brakes;
static int32_t previous_spin;
static unsigned gpio_mask;
uint32_t HAL_GetTick(void) { return tick; }
int HAL_GPIO_ReadPin(GPIO_TypeDef *p,uint16_t n) { (void)p; return (gpio_mask & n)?0:1; }
void HAL_GPIO_Init(GPIO_TypeDef *p,GPIO_InitTypeDef *g) { (void)p; (void)g; }
void HAL_GPIO_WritePin(GPIO_TypeDef *p,uint16_t n,GPIO_PinState s)
{ assert(p==Buzzer_GPIO_Port && n==Buzzer_Pin); if(s && !buzzer) ++attacks; buzzer=s; }
int32_t DriveBase_EquivalentCpsFromPwm(int16_t p) { return p; }
void DriveBase_PrepareLineTurnAssist(int32_t l,int32_t r) { (void)l; (void)r; }
void DriveBase_SetLineFaultObservation(uint8_t e,uint8_t s,uint8_t r)
{ (void)e; (void)s; (void)r; }
void DriveBase_GetTelemetry(DriveBaseTelemetry *t) { *t=telemetry; }
uint8_t DriveBase_GetFaultMask(void) { return telemetry.fault_mask; }
void DriveBase_Task(uint32_t now)
{ if(telemetry.mode==DRIVE_BASE_BRAKING && now-brake_started>=52) telemetry.mode=DRIVE_BASE_STOPPED; }
void DriveBase_Stop(DriveStopMode mode)
{
  if(mode==DRIVE_STOP_BRAKE) ++brakes;
  telemetry.mode=mode==DRIVE_STOP_BRAKE?DRIVE_BASE_BRAKING:DRIVE_BASE_STOPPED;
  brake_started=tick; memset(telemetry.requested_cps,0,sizeof telemetry.requested_cps);
}
void DriveBase_SetWheelCps(int32_t a,int32_t b,int32_t c,int32_t d)
{
  assert(telemetry.mode!=DRIVE_BASE_BRAKING && !telemetry.fault_mask);
  assert(a==b && c==d);
  if(!output.valid)
  {
    assert(a==-c && (a==LINE_SEARCH_TARGET_CPS || a==-LINE_SEARCH_TARGET_CPS));
    if(previous_spin && a!=previous_spin) ++reversals;
    previous_spin=a; ++spins;
  }
  telemetry.mode=DRIVE_BASE_SPEED;
  telemetry.requested_cps[0]=a; telemetry.requested_cps[1]=b;
  telemetry.requested_cps[2]=c; telemetry.requested_cps[3]=d;
}
void DriveBase_SetSideCps(int32_t l,int32_t r) { DriveBase_SetWheelCps(l,l,r,r); }
uint8_t DriveBase_StartPositionMove(const DrivePositionCommand *c)
{ (void)c; assert(!"Persistent search must not use position moves"); return 0; }
static void sample(unsigned mask,uint32_t dt,int16_t base)
{
  LineTrackingReading r={mask&1,(mask>>1)&1,(mask>>2)&1,(mask>>3)&1};
  tick+=dt; BuzzerPhrase400_Task(tick); DriveBase_Task(tick);
  line_tracking_compute(&r,base,&output);
  if(output.valid)
  {
    if(!output.left_cps && !output.right_cps) DriveBase_Stop(DRIVE_STOP_COAST);
    else DriveBase_SetSideCps(output.left_cps,output.right_cps);
  }
}
static void hold(unsigned mask,uint32_t ms)
{ while(ms) { uint32_t dt=ms>10?10:ms; sample(mask,dt,3000); ms-=dt; } }
static void reset(uint8_t forward,uint8_t smooth)
{
  line_tracking_reset(); memset(&telemetry,0,sizeof telemetry); BuzzerPhrase400_Init();
  attacks=spins=reversals=brakes=0; previous_spin=0;
  line_tracking_set_no_line_forward(forward); line_tracking_set_smooth_mode(smooth);
}
static void assert_search(void)
{
  assert(!output.valid && telemetry.mode==DRIVE_BASE_SPEED && BuzzerPhrase400_IsPlaying());
  assert(telemetry.requested_cps[0]==-telemetry.requested_cps[2] && spins && !reversals);
}
static void test_corner_edge_chatter(void)
{
  unsigned smooth,side,i;
  for(smooth=0;smooth<2;++smooth) for(side=0;side<2;++side)
  {
    unsigned outer=side?8:2, pair=side?12:3, before;
    reset(0,(uint8_t)smooth); hold(5,300); hold(outer,600);
    printf("corner side=%u after 600ms: left=%ld right=%ld\n",side,
           (long)telemetry.requested_cps[0],(long)telemetry.requested_cps[2]);
    fflush(stdout);
    assert(telemetry.requested_cps[0]*telemetry.requested_cps[2]<0);
    assert(!BuzzerPhrase400_IsPlaying()); /* Outer contact is a turn, not yet loss. */
    before=brakes;
    for(i=0;i<100;++i)
    {
      hold(0,30); hold(outer,30); hold(pair,30);
      assert(!output.valid && telemetry.mode==DRIVE_BASE_SPEED);
      assert(telemetry.requested_cps[0]==(side?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS));
      assert(brakes==before && BuzzerPhrase400_IsPlaying());
    }
    for(i=0;i<20;++i)
    {
      sample(5,10,3000); hold(outer,30); /* Fleeting inner hit must not brake. */
      assert(brakes==before && telemetry.mode==DRIVE_BASE_SPEED);
    }
    hold(5,180); assert(brakes==before && output.left_cps>0 && output.right_cps>0);
    assert(!BuzzerPhrase400_IsPlaying());
    /* If low-speed capture falls back to the edge, restore a continuous turn
       without another stationary confirmation pause on that outer sensor. */
    hold(outer,30); assert(brakes==before && !output.valid);
    assert(telemetry.requested_cps[0]==(side?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS));
    hold(0,30); hold(pair,300); assert(brakes==before);
    assert(BuzzerPhrase400_IsPlaying());
    hold(5,180); assert(brakes==before && !BuzzerPhrase400_IsPlaying());
    hold(5,550); assert(output.left_cps>2400);
    line_tracking_reset();
  }
}
static void test_three_black_cancels_corner(void)
{
  reset(0,0); hold(2,100);
  assert(telemetry.requested_cps[0]<0);
  sample(7,10,3000); /* Physical X2/X1/X3 black; rightmost X4 white. */
  printf("three black after corner: left=%ld right=%ld\n",
         (long)telemetry.requested_cps[0],(long)telemetry.requested_cps[2]);
  fflush(stdout);
  assert(output.valid && output.left_cps>0 && output.left_cps==output.right_cps);
}
static void test_patterns_and_narrow_windows(void)
{
  const unsigned wide[]={6,7,9,10,11,13,14,15};
  unsigned mask,mode,i,before;
  /* Every input is classified before weighted steering or a corner latch. */
  for(mask=0;mask<16;++mask)
  {
    reset(0,0); sample(mask,1,3000);
    if(mask==0) assert(!output.valid && telemetry.mode==DRIVE_BASE_SPEED && !brakes);
    else assert(output.valid && output.left_cps>0 && output.right_cps>0);
    if(mask==2 || mask==3 || mask==8 || mask==12)
    { hold(mask,30); assert(!output.valid && telemetry.requested_cps[0]*telemetry.requested_cps[2]<0); }
  }
  for(mode=0;mode<5;++mode) for(i=0;i<sizeof wide/sizeof wide[0];++i)
  {
    reset(0,0);
    if(mode==1) hold(2,100);
    if(mode==2) hold(8,100);
    if(mode==3) sample(0,1,3000); /* Transverse evidence immediately after loss. */
    if(mode==4) { hold(0,100); sample(1,1,3000); sample(1,4,3000); }
    sample(wide[i],1,3000);
    assert(output.valid && output.left_cps==output.right_cps && output.left_cps>0);
    assert(!BuzzerPhrase400_IsPlaying() && telemetry.mode==DRIVE_BASE_SPEED);
    hold(0,80); hold(2,10);
    assert(output.valid && output.left_cps==output.right_cps && !BuzzerPhrase400_IsPlaying());
    /* A real long loss eventually returns to search, not endless forward. */
    hold(0,220); assert(!output.valid && BuzzerPhrase400_IsPlaying());
  }
  reset(0,0); hold(5,100); sample(2,1,3000); sample(7,1,3000);
  hold(0,50); sample(13,1,3000); hold(8,30); hold(0,30);
  assert(!spins && !brakes); /* Cross-strip entry/exit fragments never spin. */

  reset(0,0); hold(0,100); before=brakes;
  sample(1,1,3000); sample(0,1,3000); sample(1,100,3000);
  assert(!output.valid && BuzzerPhrase400_IsPlaying()); /* Isolated or stale samples rejected. */
  sample(0,1,3000); sample(1,1,3000); sample(1,4,3000);
  assert(output.valid && output.left_cps>0 && output.right_cps>0);
  assert(brakes==before && !BuzzerPhrase400_IsPlaying()); /* No stationary reacquisition. */
  for(i=0;i<15;++i)
  {
    hold(0,45); assert(output.valid && output.left_cps>0 && !BuzzerPhrase400_IsPlaying());
    sample(i%2?1:4,1,3000); sample(i%2?1:4,4,3000);
  }
  sample(5,1,3000); assert(output.left_cps>2400 && brakes==before);
  hold(0,180); assert(!output.valid && BuzzerPhrase400_IsPlaying());
  line_tracking_reset(); assert(!BuzzerPhrase400_IsPlaying());
  puts("PASS: all 16 masks, transverse override in 5 states, edge debounce, 4-ms middle and finite gaps");
}
static void test_single_outer_flash_search_direction(void)
{
  unsigned side,smooth,forward;
  for(side=1;side<3;++side) for(smooth=0;smooth<2;++smooth) for(forward=0;forward<2;++forward)
  {
    unsigned mask=side==1?8:2;
    reset((uint8_t)forward,(uint8_t)smooth); hold(5,300);
    sample(mask,1,3000); /* Exactly one sampled outer hit, below 4/12-ms gates. */
    assert(output.valid && output.left_cps>0 && output.right_cps>0);
    hold(0,180);
    printf("single outer=%u then loss: left=%ld right=%ld\n",mask,
           (long)telemetry.requested_cps[0],(long)telemetry.requested_cps[2]);
    fflush(stdout);
    assert_search();
    assert(telemetry.requested_cps[0]==(side==1?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS));
  }
  /* A fresh opposite flash also wins over the previous recovery direction. */
  reset(0,0); hold(0,100); sample(1,1,3000); sample(1,4,3000);
  sample(8,1,3000); hold(0,180);
  assert(telemetry.requested_cps[0]==LINE_SEARCH_TARGET_CPS);
  /* Wide evidence clears earlier hints; a later tail supplies the exit side. */
  reset(0,0); hold(5,100); sample(8,1,3000); sample(7,1,3000);
  hold(0,40); sample(8,1,3000); hold(0,220);
  assert(telemetry.requested_cps[0]==LINE_SEARCH_TARGET_CPS);
  /* A sustained return to centre, an expired hint, or mode reset invalidates it. */
  reset(0,0); sample(8,1,3000); hold(5,100); hold(0,180);
  assert(telemetry.requested_cps[0]==-LINE_SEARCH_TARGET_CPS);
  reset(0,0); sample(8,1,3000); sample(0,250,3000); hold(0,100);
  assert(telemetry.requested_cps[0]==-LINE_SEARCH_TARGET_CPS);
  reset(0,0); sample(8,1,3000); reset(0,0); hold(0,100);
  assert(telemetry.requested_cps[0]==-LINE_SEARCH_TARGET_CPS);
  tick=UINT32_MAX-20; reset(0,0); sample(8,1,3000); hold(0,180);
  assert(telemetry.requested_cps[0]==LINE_SEARCH_TARGET_CPS);
}
static void test_direction_after_unconfirmed_middle(void)
{
  unsigned i,before;
  LineSearchRecord decision;
  reset(0,0); hold(2,100); /* Earlier left corner still owns recovery. */
  sample(8,1,3000); hold(0,100);
  printf("locked left, last right edge: left=%ld\n",(long)telemetry.requested_cps[0]);
  fflush(stdout);
  assert(telemetry.requested_cps[0]==LINE_SEARCH_TARGET_CPS);
  assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
  assert(decision.source==LINE_SEARCH_CORRECTION && decision.chosen_side==1);
  before=brakes;
  /* Repeated failed captures without reset must not retain the first side. */
  for(i=0;i<100;++i)
  {
    unsigned edge=i%2?8:2;
    int32_t expected=i%2?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS;
    sample(5,1,3000); /* Too brief to capture, but arms exit-direction evidence. */
    hold(0,10); /* Narrow sensors may see a white gap before the outer edge. */
    sample(edge,1,3000); hold(0,100);
    assert(!output.valid && telemetry.requested_cps[0]==expected);
    assert(brakes==before && BuzzerPhrase400_IsPlaying());
    /* Outer-only noise after loss cannot repeatedly reverse the search. */
    sample(edge==8?2:8,1,3000); hold(0,100);
    assert(telemetry.requested_cps[0]==expected && brakes==before);
  }
  /* Capture must transfer the corrected direction to the tracking wrapper. */
  sample(5,1,3000); sample(5,4,3000);
  assert(output.valid && !BuzzerPhrase400_IsPlaying());
  sample(0,250,3000); hold(0,100);
  assert(telemetry.requested_cps[0]==LINE_SEARCH_TARGET_CPS);
  /* A last edge immediately after initial white can correct the first spin. */
  reset(0,0); sample(0,1,3000); sample(8,1,3000); hold(0,100);
  assert(telemetry.requested_cps[0]==LINE_SEARCH_TARGET_CPS);
  line_tracking_reset(); assert(!BuzzerPhrase400_IsPlaying());
  reset(0,0); hold(0,100); assert(telemetry.requested_cps[0]==-LINE_SEARCH_TARGET_CPS);
  sample(5,1,3000); hold(0,250); sample(8,1,3000); hold(0,100);
  assert(telemetry.requested_cps[0]==-LINE_SEARCH_TARGET_CPS); /* Stale middle is not a new exit. */
  tick=UINT32_MAX-20; reset(0,0); hold(2,30);
  sample(5,1,3000); hold(0,10); sample(8,1,3000); hold(0,100);
  assert(telemetry.requested_cps[0]==LINE_SEARCH_TARGET_CPS);
  puts("PASS: active corner, 100 failed captures without reset, guarded correction, rolling capture handoff");
}
static void background_sample(unsigned mask, unsigned ms)
{
  gpio_mask=mask; uwTick=tick;
  while(ms--) { HAL_IncTick(); tick=uwTick; }
}
static void test_sampling_during_blocked_main(void)
{
  unsigned side,delay,repeat;
  LineSensorSample captured;
  uint32_t black_time;
  LineSensorSample_Start();
  background_sample(15U, 1U);
  black_time = tick;
  background_sample(0U, 80U);
  {
    uint32_t observed;
    assert(LineSensorSample_TakeAllBlack(&observed) && observed == black_time);
    assert(!LineSensorSample_TakeAllBlack(&observed));
    assert(LineSensorSample_Pop(&captured) && captured.mask == 15U);
    background_sample(15U, 1U);
    LineSensorSample_Reset();
    assert(!LineSensorSample_TakeAllBlack(&observed));
  }
  /* Exercise the real HAL tick override and GPIO acquisition, not injected
     controller input, while the main loop cannot call line_tracking_compute. */
  LineSensorSample_Start();
  for(side=0;side<2;++side) for(delay=5;delay<=80;delay+=15)
  {
    reset(0,1); hold(5,1000); /* Fully accelerated straight run. */
    for(repeat=0;repeat<10;++repeat)
    {
      background_sample(side?8:2,1); background_sample(0,delay);
      sample(0,0,3000); hold(0,180);
      assert(telemetry.requested_cps[0]==(side?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS));
      hold(5,800);
    }
  }
  /* Preserve ordering; do not OR a narrow edge together with a transverse mark. */
  reset(0,1); hold(5,1000);
  background_sample(8,1); background_sample(7,1); background_sample(8,1); background_sample(0,10);
  sample(0,0,3000);
  assert(output.valid && output.left_cps==output.right_cps && output.left_cps>0);
  hold(0,250); assert(telemetry.requested_cps[0]==LINE_SEARCH_TARGET_CPS);
  /* A whole middle/white/right/white sequence missed by main corrects active search. */
  reset(0,0); hold(0,100);
  background_sample(5,1); background_sample(0,10); background_sample(8,1); background_sample(0,10);
  sample(0,0,3000); hold(0,100);
  assert(telemetry.requested_cps[0]==LINE_SEARCH_TARGET_CPS);
  assert(!output.valid && BuzzerPhrase400_IsPlaying()); /* History does not fake capture. */
  /* Reset discards queued motion evidence; overflow keeps newest bounded history. */
  background_sample(8,1); reset(0,0); assert(!LineSensorSample_Pop(&captured));
  background_sample(8,1); background_sample(0,300);
  assert(LineSensorSample_Overwritten()==45U);
  sample(0,0,3000); hold(0,100);
  assert(telemetry.requested_cps[0]==-LINE_SEARCH_TARGET_CPS);
  tick=UINT32_MAX-5; reset(0,1); background_sample(8,1); background_sample(0,20);
  sample(0,0,3000); hold(0,100);
  assert(telemetry.requested_cps[0]==LINE_SEARCH_TARGET_CPS);
  puts("PASS: real tick/GPIO, accelerated straight, 1-ms edges in 5..80-ms main stalls, ordered filtering, overflow/reset/wrap");
}
static unsigned interrupt_edge;
static void enqueue_edge_on_irq_restore(void)
{
  test_irq_restore_hook=0;
  background_sample(interrupt_edge,1);
}
static void test_queue_handoff_interrupt(void)
{
  unsigned side,queued,repeat;
  LineSensorSample observation;
  LineSensorSample_Start();
  for(side=0;side<2;++side) for(queued=0;queued<2;++queued)
  {
    reset(0,1); hold(5,1000);
    for(repeat=0;repeat<20;++repeat)
    {
      if(queued) background_sample(5,1);
      interrupt_edge=side?8:2;
      test_irq_restore_hook=enqueue_edge_on_irq_restore;
      sample(0,0,3000); /* ISR runs after a queue pop releases its critical section. */
      assert(!test_irq_restore_hook);
      background_sample(0,1); sample(0,0,3000); hold(0,180);
      assert(telemetry.requested_cps[0]==(side?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS));
      hold(5,800);
    }
  }
  reset(0,0); tick=UINT32_MAX-1;
  background_sample(8,1); background_sample(0,1);
  assert(LineSensorSample_PopThrough(&observation,UINT32_MAX));
  assert(observation.mask==8 && observation.time_ms==UINT32_MAX);
  assert(!LineSensorSample_PopThrough(&observation,UINT32_MAX));
  assert(LineSensorSample_PopThrough(&observation,0));
  assert(observation.mask==0 && observation.time_ms==0);
  puts("PASS: ISR after empty/nonempty pop, 80 repeated handoffs, bounded queue across tick wrap");
}
static void test_fast_exit_after_transverse(void)
{
  unsigned side,buffered;
  LineSearchRecord decision;
  LineSensorSample_Start();
  for(side=0;side<2;++side) for(buffered=0;buffered<2;++buffered)
  {
    reset(0,1); hold(5,1000);
    if(buffered)
    {
      background_sample(7,1); background_sample(side?8:2,1); background_sample(0,20);
      sample(0,0,3000);
    }
    else { sample(7,1,3000); sample(side?8:2,1,3000); hold(0,20); }
    assert(output.valid && output.left_cps>0 && output.left_cps==output.right_cps);
    assert(!BuzzerPhrase400_IsPlaying());
    hold(0,200);
    printf("cross tail side=%u queued=%u: left=%ld\n",side,buffered,(long)telemetry.requested_cps[0]);
    fflush(stdout);
    assert(telemetry.requested_cps[0]==(side?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS));
    assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
    assert(decision.source==LINE_SEARCH_HINT && decision.chosen_side==(side?1:-1));
    assert(decision.edge_mask==(side?8:2) && decision.wide_mask==7);
  }
  /* A later broad mark still invalidates a previously seen edge. The log
     distinguishes discarded evidence from never seeing an edge at all. */
  reset(0,1); hold(5,1000); sample(8,1,3000); sample(7,1,3000); hold(0,220);
  assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
  assert(decision.source==LINE_SEARCH_DEFAULT && decision.edge_mask==8 && decision.wide_mask==7);
  assert(decision.edge_age_ms>decision.wide_age_ms && decision.chosen_side==-1);
  reset(0,1); hold(5,1000); sample(7,1,3000); sample(8,1,3000); hold(5,500); hold(0,180);
  assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
  assert(decision.source==LINE_SEARCH_DEFAULT); /* Stable centre invalidates tail direction. */
  reset(0,1); hold(5,1000); sample(7,1,3000); sample(8,1,3000); sample(0,250,3000); hold(0,100);
  assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
  assert(decision.source==LINE_SEARCH_DEFAULT && decision.edge_age_ms>=250);
  { uint32_t preserved=LineFaultLog_SearchCount(); line_tracking_reset(); assert(LineFaultLog_SearchCount()==preserved); }
}
static void test_external_brake_ownership(void)
{
  reset(0,0); hold(5,100);
  tick+=70; /* expire the narrow white-gap allowance before external braking */
  DriveBase_Stop(DRIVE_STOP_BRAKE);
  sample(0,1,3000);
  assert(telemetry.mode==DRIVE_BASE_BRAKING && brakes==1 && !output.valid);
  hold(0,40);
  assert(telemetry.mode==DRIVE_BASE_BRAKING && !spins);
  hold(0,20);
  assert_search(); assert(brakes==1);
  line_tracking_reset();
  assert(telemetry.mode==DRIVE_BASE_STOPPED && !BuzzerPhrase400_IsPlaying());
  puts("PASS: rolling search respects external brake and reset ownership");
}

int main(void)
{
  unsigned smooth,forward,i;
  test_external_brake_ownership();
  test_fast_exit_after_transverse();
  test_queue_handoff_interrupt();
  test_sampling_during_blocked_main();
  test_direction_after_unconfirmed_middle();
  test_single_outer_flash_search_direction();
  test_three_black_cancels_corner();
  test_patterns_and_narrow_windows();
  test_corner_edge_chatter();
  for(smooth=0;smooth<=1;++smooth) for(forward=0;forward<=1;++forward)
  {
    reset((uint8_t)forward,(uint8_t)smooth); hold(5,300); sample(0,70,3000);
    assert(!output.valid && telemetry.mode==DRIVE_BASE_SPEED && !brakes && BuzzerPhrase400_IsPlaying());
    hold(0,90000); assert_search(); assert(attacks>250); /* Beyond 8 s and 24 phrase repeats. */
    hold(2,10000); assert_search(); hold(8,10000); assert_search();
    hold(3,1000); assert_search(); /* Same-side pair remains an edge. */
    sample(5,10,3000); assert(telemetry.mode==DRIVE_BASE_SPEED);
    hold(0,200); assert_search(); /* False contact resumes, never latches stop. */
    hold(5,180); assert(output.valid && output.left_cps>0 && !BuzzerPhrase400_IsPlaying() && !buzzer);
    hold(2,2000); assert(!output.valid && telemetry.requested_cps[0]<0 && telemetry.requested_cps[2]>0);
    hold(5,750); assert(output.left_cps>2400 && !BuzzerPhrase400_IsPlaying());
    sample(0,10,3000); hold(0,160); assert_search();
    /* Same cancellation hook that main calls on remote STOP / mode handoff. */
    line_tracking_reset(); assert(telemetry.mode==DRIVE_BASE_STOPPED && !BuzzerPhrase400_IsPlaying() && !buzzer);
    for(i=0;i<100;++i) sample(5,10,0);
    assert(output.valid && !output.left_cps && !BuzzerPhrase400_IsPlaying());

    /* Last confirmed right hint determines rotation, not timed side swapping. */
    reset((uint8_t)forward,(uint8_t)smooth); hold(4,40); sample(0,10,3000); hold(0,500);
    assert_search(); assert(telemetry.requested_cps[0]>0);
    sample(5,10,3000); line_tracking_reset();
    assert(!BuzzerPhrase400_IsPlaying() && telemetry.mode==DRIVE_BASE_STOPPED);
    /* All real drive faults still stop audio and movement; never clear/retry. */
    for(i=0;i<3;++i)
    {
      reset(0,(uint8_t)smooth); hold(0,100);
      telemetry.fault_mask=(uint8_t)(i==0?1:(i==1?32:64)); sample(0,10,3000);
      assert(output.valid && !output.left_cps && !BuzzerPhrase400_IsPlaying());
      assert(LineRecovery_GetStopReason()==LINE_REC_STOP_DRIVE_FAULT);
      telemetry.fault_mask=0; hold(5,1000);
      assert(!output.left_cps && !BuzzerPhrase400_IsPlaying());
    }
  }
  /* Verify the actual predefined envelope, not a replacement alarm pattern. */
  reset(0,0); sample(0,0,3000); assert(buzzer==GPIO_PIN_SET);
  hold(0,110); assert(!buzzer); hold(0,30); assert(buzzer);
  hold(0,150); assert(!buzzer); hold(0,10); assert(buzzer);
  hold(0,140); assert(!buzzer); hold(0,60); assert(buzzer);
  hold(0,170); assert(!buzzer); hold(0,12); assert(buzzer);
  hold(0,280); assert(!buzzer); hold(0,568); assert(buzzer && attacks==6);
  /* Normal mode reset must not claim an unrelated manually started phrase. */
  line_tracking_reset(); BuzzerPhrase400_Start(1); line_tracking_reset();
  assert(BuzzerPhrase400_IsPlaying()); BuzzerPhrase400_Stop();
  reset(1,1); sample(0,10,3000); assert(output.left_cps>0 && !BuzzerPhrase400_IsPlaying());
  tick=UINT32_MAX-500; reset(0,1); hold(0,10000); assert_search();
  hold(5,700); assert(output.left_cps>2400 && !BuzzerPhrase400_IsPlaying());
  printf("PASS CPS=%ld: persistent rotation, actual repeating phrase, capture/re-loss, reset/STOP, faults, wrap\n",(long)LINE_SEARCH_TARGET_CPS);
  return 0;
}
