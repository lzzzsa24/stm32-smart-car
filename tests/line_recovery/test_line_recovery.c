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
static uint8_t manual_phrase_test;
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
  if(!manual_phrase_test) assert(!BuzzerPhrase400_IsPlaying() && !buzzer);
  if(output.valid)
  {
    if(!output.left_cps && !output.right_cps) DriveBase_Stop(DRIVE_STOP_COAST);
    else DriveBase_SetSideCps(output.left_cps,output.right_cps);
  }
}
static void hold(unsigned mask,uint32_t ms)
{ while(ms) { uint32_t dt=ms>10?10:ms; sample(mask,dt,3000); ms-=dt; } }
static void advance_all_encoder_transitions(uint32_t count)
{
  unsigned wheel;
  for(wheel=0;wheel<DRIVE_BASE_WHEEL_COUNT;++wheel)
    telemetry.legal_transition_count[wheel]+=count;
}
static void reset(uint8_t forward,uint8_t smooth)
{
  line_tracking_reset(); memset(&telemetry,0,sizeof telemetry); BuzzerPhrase400_Init();
  attacks=spins=reversals=brakes=0; previous_spin=0;
  line_tracking_set_no_line_forward(forward); line_tracking_set_smooth_mode(smooth);
}
static void assert_search(void)
{
  assert(!output.valid && telemetry.mode==DRIVE_BASE_SPEED && LineRecovery_IsSearching());
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
    assert(output.valid && output.left_cps==(side?2200:-2200) && output.right_cps==-output.left_cps);
    assert(!BuzzerPhrase400_IsPlaying()); /* Outer contact is a turn, not yet loss. */
    before=brakes;
    for(i=0;i<100;++i)
    {
      hold(0,100);
      assert(telemetry.requested_cps[0]==(side?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS));
      hold(outer,30); hold(pair,30);
      assert(output.valid && telemetry.mode==DRIVE_BASE_SPEED);
      assert(telemetry.requested_cps[0]>=0 && telemetry.requested_cps[2]>=0 && telemetry.requested_cps[0]+telemetry.requested_cps[2]>0);
      assert(brakes==before && LineRecovery_IsSearching());
    }
    for(i=0;i<20;++i)
    {
      sample(5,10,3000); hold(outer,30); /* Fleeting inner hit must not brake. */
      assert(brakes==before && telemetry.mode==DRIVE_BASE_SPEED);
    }
    hold(5,180); assert(brakes==before && output.left_cps>=0 && output.right_cps>=0 && output.left_cps+output.right_cps>0);
    assert(!BuzzerPhrase400_IsPlaying());
    /* If low-speed capture falls back to the edge, restore a continuous turn
       without another stationary confirmation pause on that outer sensor. */
    hold(outer,30); assert(brakes==before && output.valid);
    assert(telemetry.requested_cps[0]>=0 && telemetry.requested_cps[2]>=0 && telemetry.requested_cps[0]+telemetry.requested_cps[2]>0);
    hold(0,100); hold(pair,300); assert(brakes==before);
    assert(LineRecovery_IsSearching());
    hold(5,180); assert(brakes==before && !BuzzerPhrase400_IsPlaying());
    hold(5,550); assert(output.left_cps>2400);
    line_tracking_reset();
  }
}
static void test_three_black_cancels_corner(void)
{
  reset(0,0); hold(2,100); hold(0,100);
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
    else assert(output.valid && output.left_cps>=0 && output.right_cps>=0 && output.left_cps+output.right_cps>0);
    if(mask==2 || mask==3 || mask==8 || mask==12)
    { hold(mask,30); assert(output.valid && telemetry.requested_cps[0]>=0 && telemetry.requested_cps[2]>=0 && telemetry.requested_cps[0]+telemetry.requested_cps[2]>0); }
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
    hold(0,220); assert(!output.valid && LineRecovery_IsSearching());
  }
  reset(0,0); hold(5,100); sample(2,1,3000); sample(7,1,3000);
  hold(0,50); sample(13,1,3000); hold(8,30); hold(0,30);
  assert(!spins && !brakes); /* Cross-strip entry/exit fragments never spin. */

  reset(0,0); hold(0,100); before=brakes;
  sample(1,1,3000); sample(0,1,3000); sample(1,100,3000);
  assert(output.valid && LineRecovery_IsSearching()); /* Forward contact is not confirmed capture. */
  sample(0,1,3000); sample(1,1,3000); sample(1,4,3000);
  assert(output.valid && output.left_cps>=0 && output.right_cps>=0 && output.left_cps+output.right_cps>0);
  assert(brakes==before && !BuzzerPhrase400_IsPlaying()); /* No stationary reacquisition. */
  for(i=0;i<15;++i)
  {
    hold(0,45); assert(output.valid && output.left_cps>0 && !BuzzerPhrase400_IsPlaying());
    sample(i%2?1:4,1,3000); sample(i%2?1:4,4,3000);
  }
  sample(5,1,3000); assert(output.left_cps>2400 && brakes==before);
  hold(0,180); assert(!output.valid && LineRecovery_IsSearching());
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
    assert(output.valid && output.left_cps>=0 && output.right_cps>=0 && output.left_cps+output.right_cps>0);
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
  /* Wide evidence suppresses turning; a later tail refreshes the exit side. */
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
static void test_lone_inner_uses_encoder_bounded_probe(void)
{
  unsigned mirror;
  for(mirror=0;mirror<2;++mirror)
  {
    unsigned mask=mirror?4U:1U;
    int32_t initial=mirror?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS;
    int32_t reversed=-initial;
    unsigned before_reversals;
    LineSearchRecord decision;

    reset(0,1);
    hold(5,100);
    hold(mask,40);
    hold(0,180);
    assert(!output.valid && telemetry.mode==DRIVE_BASE_SPEED);
    assert(telemetry.requested_cps[0]==initial);
    assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
    assert(decision.source==LINE_SEARCH_INNER_PROBE);
    assert(decision.hint_mask==mask);
    assert(decision.chosen_side==(mirror?1:-1));

    before_reversals=reversals;
    advance_all_encoder_transitions(100000U);
    sample(0,1,3000);
    assert(telemetry.requested_cps[0]==reversed);
    assert(reversals==before_reversals+1U);

    /* Time alone must not cause another reversal; each sweep is bounded by
       measured motion from all four repaired wheel encoders. */
    hold(0,5000);
    assert(telemetry.requested_cps[0]==reversed);
    assert(reversals==before_reversals+1U);

    /* Re-contact on the same lone inner sensor is still ambiguous. If it is
       lost during settle, retain the direction that physically found it. */
    sample(mask,1,3000);
    sample(mask,4,3000);
    assert(output.valid && !BuzzerPhrase400_IsPlaying());
    hold(0,250);
    assert(!output.valid && telemetry.requested_cps[0]==reversed);
  }
  puts("PASS: lone X1/X3 loss uses mirrored encoder-bounded expanding probes");
  line_tracking_reset();
}
static void test_direction_after_unconfirmed_middle(void)
{
  unsigned i,before;
  LineSearchRecord decision;
  reset(0,0); hold(2,100); hold(0,100); /* Earlier left search owns recovery. */
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
    assert(brakes==before && LineRecovery_IsSearching());
    /* A newer outer exit must win even without an intervening middle hit. */
    sample(edge==8?2:8,1,3000); hold(0,100);
    assert(telemetry.requested_cps[0]==-expected && brakes==before);
  }
  /* Capture must transfer the corrected direction to the tracking wrapper. */
  sample(5,1,3000); sample(5,4,3000);
  assert(output.valid && !BuzzerPhrase400_IsPlaying());
  sample(0,250,3000); hold(0,100);
  assert(telemetry.requested_cps[0]==-LINE_SEARCH_TARGET_CPS);
  /* A last edge immediately after initial white can correct the first spin. */
  reset(0,0); sample(0,1,3000); sample(8,1,3000); hold(0,100);
  assert(telemetry.requested_cps[0]==LINE_SEARCH_TARGET_CPS);
  line_tracking_reset(); assert(!BuzzerPhrase400_IsPlaying());
  reset(0,0); hold(0,100); assert(telemetry.requested_cps[0]==-LINE_SEARCH_TARGET_CPS);
  sample(5,1,3000); hold(0,250); sample(8,1,3000); hold(0,100);
  assert(telemetry.requested_cps[0]==LINE_SEARCH_TARGET_CPS); /* Fresh edge does not need a recent middle. */
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
  assert(!output.valid && LineRecovery_IsSearching()); /* History does not fake capture. */
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
  /* A broad mark cancels turning but retains a bounded recent side. The log
     distinguishes this held hint from defaulting without directional evidence. */
  reset(0,1); hold(5,1000); sample(8,1,3000); sample(7,1,3000); hold(0,220);
  assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
  assert(decision.source==LINE_SEARCH_CROSS_HINT && decision.edge_mask==8 && decision.wide_mask==7);
  assert(decision.edge_age_ms>decision.wide_age_ms && decision.chosen_side==-1);
  assert(decision.hint_mask==7); /* New adjacent triple supersedes the old right edge. */
  reset(0,1); hold(5,1000); sample(7,1,3000); sample(8,1,3000); hold(5,500); hold(0,180);
  assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
  assert(decision.source==LINE_SEARCH_DEFAULT); /* Stable centre invalidates tail direction. */
  reset(0,1); hold(5,1000); sample(7,1,3000); sample(8,1,3000); sample(0,250,3000); hold(0,100);
  assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
  assert(decision.source==LINE_SEARCH_DEFAULT && decision.edge_age_ms>=250);
  { uint32_t preserved=LineFaultLog_SearchCount(); line_tracking_reset(); assert(LineFaultLog_SearchCount()==preserved); }
}
static void test_latest_outer_after_long_search(void)
{
  unsigned first,queued,i,failures=0;
  LineSensorSample_Start();
  for(first=0;first<2;++first) for(queued=0;queued<2;++queued)
  {
    reset(0,0); hold(first?8:2,30); hold(0,500);
    /* The old one-shot exit window is now consumed and expired. No middle
       is supplied: each newly observed outer edge must still be usable. */
    for(i=0;i<12;++i)
    {
      unsigned right=(first+i+1)%2,edge=right?8:2;
      int32_t expected=right?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS;
      if(queued)
      { background_sample(edge,1); background_sample(0,20); sample(0,0,3000); }
      else
      { sample(edge,1,3000); sample(0,1,3000); }
      if(telemetry.requested_cps[0]!=expected) ++failures;
      assert(!output.valid && LineRecovery_IsSearching() && !brakes);
      hold(0,300);
    }
  }
  printf("Fresh outer after consumed/expired window: wrong=%u/48\n",failures);
  fflush(stdout); assert(failures==0);
  reset(0,0); hold(2,30); hold(0,300);
  sample(8,1,3000);
  assert(output.valid && telemetry.requested_cps[0]>=0 && telemetry.requested_cps[2]>=0 && telemetry.requested_cps[0]+telemetry.requested_cps[2]>0); /* visible contact advances */
  sample(0,201,3000);
  assert(telemetry.requested_cps[0]==-LINE_SEARCH_TARGET_CPS); /* stale edge */
  sample(8,1,3000); sample(5,1,3000); sample(0,1,3000);
  assert(telemetry.requested_cps[0]==LINE_SEARCH_TARGET_CPS); /* unconfirmed middle preserves the fresh exit */
  sample(8,1,3000); sample(2,1,3000); sample(0,1,3000);
  assert(telemetry.requested_cps[0]==-LINE_SEARCH_TARGET_CPS); /* latest edge wins */
  sample(8,1,3000); sample(0,1,3000);
  assert(telemetry.requested_cps[0]==LINE_SEARCH_TARGET_CPS);
  line_tracking_reset();
}

static void test_alternating_corner_handoffs(void)
{
  unsigned first,iteration,queued,mismatches=0;
  tick=UINT32_MAX-100;
  LineSensorSample_Start();
  for(first=0;first<2;++first)
  {
    reset(0,0); hold(first?8:2,30); hold(0,100);
    /* No reset between contacts: a one-sided inner capture is ambiguous, so
       re-loss retains the search direction that physically found it. */
    for(iteration=0;iteration<12;++iteration)
    {
      unsigned right=(first+iteration+1)%2;
      int32_t expected=first?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS;
      sample(right?4:1,1,3000); sample(right?4:1,4,3000);
      assert(output.valid && !BuzzerPhrase400_IsPlaying());
      sample(0,61,3000);
      if(telemetry.requested_cps[0]!=expected) ++mismatches;
    }
  }
  for(first=0;first<2;++first) for(queued=0;queued<2;++queued)
  {
    unsigned inner=first?1:4;
    int32_t expected=first?-LINE_SEARCH_TARGET_CPS:LINE_SEARCH_TARGET_CPS;
    reset(0,0); hold(first?8:2,30); hold(0,100);
    sample(5,1,3000); sample(5,4,3000); /* capture starts at t=0 */
    if(queued)
    {
      hold(5,480);
      background_sample(5,15); background_sample(first?2:8,1);
      background_sample(inner,4); sample(inner,0,3000);
    }
    else
    {
      hold(5,490);
      sample(inner,1,3000); sample(inner,4,3000); /* new side confirmed at t=495 */
      sample(inner,5,3000); /* t=500: transition into normal tracking */
    }
    sample(0,61,3000);
    if(telemetry.requested_cps[0]!=expected) ++mismatches;
  }
  printf("Ambiguous capture/normal probe handoffs: wrong directions=%u (live/queued, tick wrap)\n",mismatches);
  fflush(stdout);
  assert(mismatches==0);
  line_tracking_reset();
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

static void test_gpio_snapshot_before_interrupt(void)
{
  unsigned active, right, wrap, stale, at_restore, cases=0, failures=0;
  LineSensorSample_Start();
  for(active=0;active<2;++active) for(right=0;right<2;++right) for(wrap=0;wrap<2;++wrap)
  for(stale=0;stale<4;++stale) for(at_restore=0;at_restore<2;++at_restore)
  {
    LineTrackingReading snapshot;
    tick=wrap?UINT32_MAX-500U:1000U;
    reset(0,1);
    if(active) { hold(right?2:8,30); hold(0,300); }
    else hold(5,400);
    if(wrap) tick=UINT32_MAX;
    /* Previous inner, centered, opposite outer and wide snapshots can all
       erase a newer edge if history is drained through compute time. */
    gpio_mask=stale==0?(right?1:4):(stale==1?5:(stale==2?(right?2:8):15));
    interrupt_edge=right?8:2;
    if(at_restore) test_irq_restore_hook=enqueue_edge_on_irq_restore;
    snapshot=line_tracking_read();
    if(!at_restore) background_sample(interrupt_edge,1);
    assert(!test_irq_restore_hook);
    line_tracking_compute(&snapshot,3000,&output);
    line_tracking_apply_command(&output,3599);
    background_sample(0,1);
    snapshot=line_tracking_read();
    line_tracking_compute(&snapshot,3000,&output);
    line_tracking_apply_command(&output,3599);
    hold(0,150);
    if(telemetry.requested_cps[0]!=(right?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS)) ++failures;
    ++cases;
  }
  printf("GPIO snapshot before newer ISR edge: wrong=%u/%u\n",failures,cases);
  fflush(stdout); assert(failures==0);
  /* Within the same tick the direct snapshot is later than queued history.
     Tick zero is a valid timestamp, not a synthetic-reading sentinel. */
  for(right=0;right<2;++right)
  {
    LineTrackingReading snapshot;
    tick=UINT32_MAX-1U; reset(0,1);
    background_sample(right?2:8,1);
    background_sample(right?2:8,1);
    gpio_mask=right?8:2;
    snapshot=line_tracking_read();
    assert(snapshot.sampled_time_valid && snapshot.sampled_ms==0U);
    line_tracking_compute(&snapshot,3000,&output);
    background_sample(0,1);
    snapshot=line_tracking_read();
    line_tracking_compute(&snapshot,3000,&output);
    assert(telemetry.requested_cps[0]==(right?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS));
  }
  line_tracking_reset();
}

static void test_direction_survives_short_wide_mark(void)
{
  unsigned right, active, queued, wrap, wrong=0;
  LineSearchRecord decision;
  LineSensorSample_Start();
  for(right=0;right<2;++right) for(active=0;active<2;++active)
  for(queued=0;queued<2;++queued) for(wrap=0;wrap<2;++wrap)
  {
    unsigned edge=right?8:2;
    tick=wrap?UINT32_MAX-(active?330U:400U)-80U:1000U;
    reset(0,1);
    if(active) { hold(right?2:8,30); hold(0,300); }
    else hold(5,400);
    if(queued)
    {
      background_sample(edge,1); background_sample(15,120);
      background_sample(5,20); background_sample(0,10);
      sample(0,0,3000);
    }
    else
    {
      sample(edge,1,3000); hold(15,120);
      assert(output.valid && output.left_cps==output.right_cps && !BuzzerPhrase400_IsPlaying());
      hold(5,20); hold(0,10);
    }
    hold(0,150);
    if(telemetry.requested_cps[0]!=(right?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS)) ++wrong;
    assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
    assert(decision.source==LINE_SEARCH_CROSS_HINT && decision.hint==(right?1:-1));
  }
  printf("Recent side -> wide -> brief center -> loss: wrong=%u/16\n",wrong);
  fflush(stdout); assert(wrong==0);
  /* A horizontal strip followed by a lasting centre is still ordinary
     forward driving. Neither wide repeats nor centre readings renew a hold. */
  for(right=0;right<2;++right) for(queued=0;queued<2;++queued)
  {
    unsigned edge=right?8:2, opposite=right?2:8;
    reset(0,1); sample(edge,1,3000); hold(15,120);
    if(queued) { background_sample(5,95); sample(5,0,3000); }
    else hold(5,95);
    assert(output.valid && output.left_cps>=0 && output.right_cps>=0 && output.left_cps+output.right_cps>0 && !spins);
    hold(0,120);
    assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
    assert(decision.source==LINE_SEARCH_DEFAULT && decision.hint==0);

    reset(0,1); sample(edge,1,3000); hold(15,410); hold(0,120);
    assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
    assert(decision.source==LINE_SEARCH_DEFAULT); /* 400-ms absolute expiry */

    reset(0,1); sample(edge,1,3000); sample(15,201,3000); hold(0,120);
    assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
    assert(decision.source==LINE_SEARCH_DEFAULT); /* no revival at wide entry */

    reset(0,1); sample(edge,1,3000); hold(15,120);
    if(queued) { background_sample(opposite,1); background_sample(0,10); sample(0,0,3000); }
    else { sample(opposite,1,3000); hold(0,10); }
    assert(output.valid && output.left_cps==output.right_cps && !spins);
    hold(0,150);
    assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
    assert(decision.source==LINE_SEARCH_HINT && decision.chosen_side==(right?-1:1));

    reset(0,1); sample(edge,1,3000); hold(15,120);
    if(queued) { background_sample(right?1:4,8); background_sample(0,10); sample(0,0,3000); }
    else { hold(right?1:4,8); sample(right?1:4,4,3000); hold(0,10); }
    hold(0,150);
    assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
    /* A lone inner contact cannot overturn the still-fresh, unambiguous
       outer-edge direction retained across the transverse strip. */
    assert(decision.source==LINE_SEARCH_CROSS_HINT &&
           decision.hint_mask==edge && decision.chosen_side==(right?1:-1));

    reset(0,1); sample(edge,1,3000); hold(15,120); reset(0,1); hold(0,120);
    assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
    assert(decision.source==LINE_SEARCH_DEFAULT); /* mode/STOP clears hold */
  }
  reset(0,1); sample(8,1,3000); hold(15,120);
  background_sample(0,300); sample(0,0,3000);
  assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
  assert(decision.source==LINE_SEARCH_DEFAULT && decision.queue_overwritten>0);
  /* No side information is invented for a symmetric wide/centre/white exit. */
  reset(0,1); hold(15,120); hold(5,20); hold(0,150);
  assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
  assert(decision.source==LINE_SEARCH_DEFAULT && decision.hint==0);
  puts("PASS: crossing hold expiry, centre clear, newest side, inner probe, reset, overflow, unknown side");
  line_tracking_reset();
}

static void test_overlapping_direction_and_interrupted_confirmation(void)
{
  unsigned right, queued, active, wrap, wrong=0, false_corner=0, false_capture=0;
  LineSearchRecord decision;
  LineSensorSample_Start();
  for(right=0;right<2;++right) for(queued=0;queued<2;++queued) for(active=0;active<2;++active)
  for(wrap=0;wrap<2;++wrap)
  {
    unsigned broad=right?13:7;
    tick=wrap?UINT32_MAX-(active?130U:30U)-10U:1000U;
    reset(0,1); hold(right?1:4,30); /* Opposite inner hint, as in the captured log. */
    if(active) hold(0,100);
    if(queued) { background_sample(broad,20); background_sample(0,20); sample(0,0,3000); }
    else { hold(broad,20); hold(0,20); }
    assert(output.valid && output.left_cps==output.right_cps && !BuzzerPhrase400_IsPlaying());
    hold(0,150);
    if(telemetry.requested_cps[0]!=(right?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS)) ++wrong;
    assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
    assert(decision.hint_mask==broad && decision.source==LINE_SEARCH_CROSS_HINT);
  }
  for(right=0;right<2;++right)
  {
    unsigned edge=right?8:2;
    reset(0,1); hold(5,100); sample(edge,1,3000);
    background_sample(0,15); background_sample(edge,1); sample(edge,0,3000);
    if(!output.valid) ++false_corner;
    hold(edge,20); assert(output.valid && output.left_cps>=0 && output.right_cps>=0 && output.left_cps+output.right_cps>0); /* Visible edge stays forward. */

    reset(0,1); hold(0,100); sample(5,1,3000);
    background_sample(right?0:8,4); background_sample(5,1); sample(5,0,3000);
    if(!LineRecovery_IsSearching()) ++false_capture;
    sample(5,4,3000); assert(output.valid); /* New continuous capture still succeeds. */
  }
  printf("Overlap wrong=%u/16; interrupted corner=%u/2; interrupted capture=%u/2\n",
         wrong,false_corner,false_capture);
  fflush(stdout); assert(!wrong && !false_corner && !false_capture);
  line_tracking_reset();
}

static unsigned mirrored_mask(unsigned mask)
{ return ((mask&1)<<2)|((mask&2)<<2)|((mask&4)>>2)|((mask&8)>>2); }

static void test_direction_pattern_matrix(void)
{
  static const int expected[16]={0,0,-1,-1,0,0,0,-1,1,0,0,0,1,1,0,0};
  unsigned mask, next, mirror, checked=0;
  assert(line_tracking_direction_evidence(0)==0);
  for(mask=0;mask<16;++mask)
  {
    LineTrackingReading r={mask&1,(mask>>1)&1,(mask>>2)&1,(mask>>3)&1};
    assert(line_tracking_direction_evidence(&r)==expected[mask]);
    assert(expected[mirrored_mask(mask)]==-expected[mask]);
    if(mask==7 || mask==13)
    {
      LineSearchRecord d;
      reset(0,1); hold(mask,500);
      assert(output.valid && output.left_cps==output.right_cps && !spins);
      hold(0,110);
      assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&d));
      assert(d.chosen_side==expected[mask] && d.hint_mask==mask);
    }
  }
  /* All ordered pattern pairs with a mirrored seed must preserve symmetry.
     Unknown-side cases intentionally share the existing default-left policy. */
  for(mask=0;mask<16;++mask) for(next=0;next<16;++next)
  {
    LineSearchRecord d[2];
    for(mirror=0;mirror<2;++mirror)
    {
      unsigned a=mirror?mirrored_mask(mask):mask, b=mirror?mirrored_mask(next):next;
      reset(0,1); hold(mirror?4:1,30);
      sample(a,1,3000); sample(b,1,3000); hold(0,180);
      assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&d[mirror]));
    }
    assert(d[0].source==d[1].source);
    assert(d[0].hint_mask==mirrored_mask(d[1].hint_mask));
    if(d[0].source!=LINE_SEARCH_DEFAULT) assert(d[0].chosen_side==-d[1].chosen_side);
    ++checked;
  }
  printf("PASS: all 16 direction patterns and %u mirrored ordered pairs\n",checked);
  line_tracking_reset();
}

static void test_strong_exit_survives_inner_contact(void)
{
  static const unsigned middles[]={1,4,5};
  unsigned right, queued, middle, wrap, wrong=0, overlap_wrong=0;
  LineSearchRecord decision;
  LineSensorSample_Start();
  for(right=0;right<2;++right) for(queued=0;queued<2;++queued)
  {
    for(middle=0;middle<3;++middle) for(wrap=0;wrap<2;++wrap)
    {
      unsigned edge=right?8:2;
      tick=wrap?UINT32_MAX-332U:1000U;
      reset(0,1); hold(right?2:8,30); hold(0,300);
      if(queued) { background_sample(edge,1); background_sample(middles[middle],1); background_sample(0,1); sample(0,0,3000); }
      else { sample(edge,1,3000); sample(middles[middle],1,3000); sample(0,1,3000); }
      if(telemetry.requested_cps[0]!=(right?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS)) ++wrong;
      assert(!output.valid && LineRecovery_IsSearching());
      assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
      assert(decision.source==LINE_SEARCH_CORRECTION && decision.chosen_side==(right?1:-1));
    }
    reset(0,1); hold(right?1:4,30);
    if(queued) { background_sample(right?9:6,1); background_sample(0,1); sample(0,0,3000); }
    else { sample(right?9:6,1,3000); sample(0,1,3000); }
    hold(0,120);
    if(telemetry.requested_cps[0]!=(right?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS)) ++overlap_wrong;
    assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
    assert(decision.hint_mask==(right?9:6) && decision.source==LINE_SEARCH_CROSS_HINT);
  }
  printf("Pending outer through inner: wrong=%u/24; ordered overlap: wrong=%u/4\n",wrong,overlap_wrong);
  fflush(stdout); assert(!wrong && !overlap_wrong);
  for(right=0;right<2;++right)
  {
    unsigned edge=right?8:2, inner=right?1:4, overlap=right?9:6;
    /* Normal tracking: lone-inner bounce never deletes a recent strong side. */
    reset(0,1); sample(edge,1,3000); sample(inner,1,3000); hold(0,120);
    assert(telemetry.requested_cps[0]==(right?LINE_SEARCH_TARGET_CPS:-LINE_SEARCH_TARGET_CPS));

    reset(0,1); hold(right?2:8,30); hold(0,300);
    sample(edge,1,3000); sample(inner,1,3000); sample(0,201,3000);
    assert(LineRecovery_GetDirection()==(right?-1:1)); /* inner does not renew 200-ms exit age */

    reset(0,1); hold(right?2:8,30); hold(0,300);
    sample(edge,1,3000); sample(inner,1,3000); sample(right?2:8,1,3000); sample(0,1,3000);
    assert(LineRecovery_GetDirection()==(right?-1:1)); /* newest outer wins */

    reset(0,1); hold(right?2:8,30); hold(0,300);
    sample(edge,1,3000); sample(inner,1,3000); sample(inner,4,3000);
    assert(output.valid && !BuzzerPhrase400_IsPlaying()); /* confirmed capture still commits */
    hold(0,120);
    assert(LineRecovery_GetDirection()==(right?-1:1)); /* no stale pending exit after capture */

    reset(0,1); hold(inner,30); reset(0,1); sample(overlap,1,3000); hold(0,120);
    assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
    assert(decision.source==LINE_SEARCH_DEFAULT); /* no transition across reset */

    reset(0,1); hold(inner,30); sample(overlap,31,3000); hold(0,120);
    assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
    assert(decision.source==LINE_SEARCH_DEFAULT); /* no transition across >30-ms observation gap */

    reset(0,1); hold(inner,30); sample(15,1,3000); sample(overlap,1,3000); hold(0,120);
    assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
    assert(decision.source==LINE_SEARCH_DEFAULT); /* broad mark interrupts the ordering evidence */

    reset(0,1); hold(inner,30); sample(overlap,1,3000); hold(overlap,410); hold(0,120);
    assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
    assert(decision.source==LINE_SEARCH_DEFAULT); /* static pair never renews inferred side */

    reset(0,1); hold(inner,30); background_sample(overlap,260); sample(0,0,3000);
    hold(0,120); assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
    assert(decision.source==LINE_SEARCH_DEFAULT && decision.queue_overwritten>0);

    tick=UINT32_MAX-32U; reset(0,1); hold(inner,30);
    background_sample(overlap,1); background_sample(0,2); sample(0,0,3000); hold(0,120);
    assert(LineFaultLog_GetSearch(LineFaultLog_SearchCount()-1,&decision));
    assert(decision.hint_mask==overlap && decision.chosen_side==(right?1:-1));
  }
  puts("PASS: strong-hint bounce protection, expiry, newer outer, capture, reset, gap, broad interruption, static overlap, overflow, wrap");
  line_tracking_reset();
}

static void test_persistent_outer_escalates(void)
{
  unsigned right;
  for(right=0;right<2;++right)
  {
    unsigned outer=right?8:2;
    reset(0,1); hold(outer,250);
    printf("persistent outer %u after 250 ms: %ld/%ld\n",outer,
        (long)output.left_cps,(long)output.right_cps); fflush(stdout);
    assert(output.valid && output.left_cps==(right?2200:-2200));
    assert(output.right_cps==-output.left_cps);
    assert(!brakes && !BuzzerPhrase400_IsPlaying());
  }
  line_tracking_reset();
}

static void test_outer_escalation_boundaries(void)
{
  unsigned right,wrap,mask,cases=0;
  LineSensorSample_Start();
  for(right=0;right<2;++right) for(wrap=0;wrap<2;++wrap)
  {
    unsigned outer=right?8:2;
    int32_t strong_left=right?2200:-2200;
    tick=wrap?UINT32_MAX-100:1000;
    reset(0,1); sample(outer,1,3000); hold(outer,119);
    assert(output.left_cps==(right?2200:0) && output.right_cps==(right?0:2200));
    sample(outer,1,3000);
    assert(output.left_cps==strong_left && output.right_cps==-strong_left);
    line_tracking_apply_command(&output,0);
    assert(telemetry.mode==DRIVE_BASE_STOPPED); /* zero cap still stops negative-target correction */
    line_tracking_reset(); sample(outer,1,3000);
    assert(output.left_cps==(right?2200:0) && output.right_cps==(right?0:2200));
    hold(outer,130); sample(outer,31,3000);
    assert(output.left_cps==(right?2200:0) && output.right_cps==(right?0:2200)); /* unobserved gap */
    for(mask=0;mask<16;++mask) if(mask!=outer)
    {
      reset(0,1); hold(outer,130);
      assert(output.left_cps==strong_left);
      sample(mask,1,3000); sample(outer,1,3000); hold(outer,110);
      assert(output.valid && output.left_cps>=0 && output.right_cps>=0);
      hold(outer,20); assert(output.left_cps==strong_left && output.right_cps==-strong_left);
      ++cases;
    }
    reset(0,1); sample(outer,1,3000); background_sample(outer,120); sample(outer,0,3000);
    assert(output.left_cps==strong_left); /* complete ISR history can prove continuity */
    background_sample(5,1); background_sample(outer,60); sample(outer,0,3000);
    assert(output.left_cps>=0 && output.right_cps>=0); /* unseen middle breaks it */
    background_sample(outer,300); background_sample(5,1); background_sample(outer,20);
    sample(outer,0,3000);
    assert(LineSensorSample_Overwritten()>0 && output.left_cps>=0 && output.right_cps>=0);
    reset(0,1); sample(outer,1,3000); background_sample(outer,119);
    {
      LineTrackingReading frozen=line_tracking_read();
      background_sample(outer,2);
      line_tracking_compute(&frozen,3000,&output);
      assert(output.left_cps>=0 && output.right_cps>=0); /* no future sample escalation */
      sample(outer,0,3000); assert(output.left_cps==strong_left);
    }
    assert(!brakes && !BuzzerPhrase400_IsPlaying());
  }
  line_tracking_reset();
  printf("PASS: %u interruptions reset escalation; 119/120 ms, tick wrap, ISR continuity/middle break, overflow, frozen snapshot, zero cap and STOP\n",cases);
}

static void test_visible_forward_and_lost_spin(void)
{
  unsigned mask, smooth, gain, active, i, cases=0;
  for(mask=1;mask<16;++mask) for(smooth=0;smooth<2;++smooth)
  for(gain=100;gain<=200;gain+=100) for(active=0;active<2;++active)
  {
    reset(0,(uint8_t)smooth); line_tracking_set_turn_gain_percent((uint16_t)gain);
    if(active) hold(0,120);
    for(i=0;i<30;++i)
    {
      sample(mask,10,3000);
      if((mask==2 || mask==8) && i>=12)
        assert(output.valid && output.left_cps==(mask==2?-2200:2200) && output.right_cps==-output.left_cps);
      else assert(output.valid && telemetry.requested_cps[0]>=0 && telemetry.requested_cps[2]>=0 && telemetry.requested_cps[0]+telemetry.requested_cps[2]>0);
      assert(!brakes);
      if(mask==2 || mask==3) assert(output.left_cps<output.right_cps);
      if(mask==8 || mask==12) assert(output.left_cps>output.right_cps);
    }
    hold(0,120);
    assert(!output.valid && LineRecovery_IsSearching());
    assert(telemetry.requested_cps[0]==-telemetry.requested_cps[2]);
    assert(telemetry.requested_cps[0]==LINE_SEARCH_TARGET_CPS || telemetry.requested_cps[0]==-LINE_SEARCH_TARGET_CPS);
    line_tracking_reset(); assert(!BuzzerPhrase400_IsPlaying() && telemetry.requested_cps[0]==0);
    ++cases;
  }
  line_tracking_set_turn_gain_percent(100);
  reset(0,1); hold(5,100); sample(0,1,3000); hold(0,49);
  assert(output.valid && output.left_cps==output.right_cps && output.left_cps>0);
  hold(0,20); assert(!output.valid && telemetry.requested_cps[0]==-telemetry.requested_cps[2]);
  reset(0,1); sample(15,1,3000); hold(0,90);
  assert(output.valid && output.left_cps==output.right_cps && output.left_cps>0);
  hold(0,20); assert(!output.valid && telemetry.requested_cps[0]==-telemetry.requested_cps[2]);
  printf("PASS: %u visible mask/gain/state cases: brief forward correction, sustained outer counter-rotation, loss search, preserved 60/100-ms gaps\n",cases);
  line_tracking_reset();
}

int main(void)
{
  unsigned smooth,forward,i;
  test_persistent_outer_escalates();
  test_outer_escalation_boundaries();
  test_visible_forward_and_lost_spin();
  test_strong_exit_survives_inner_contact();
  test_overlapping_direction_and_interrupted_confirmation();
  test_direction_pattern_matrix();
  test_direction_survives_short_wide_mark();
  test_gpio_snapshot_before_interrupt();
  test_latest_outer_after_long_search();
  test_alternating_corner_handoffs();
  test_external_brake_ownership();
  test_fast_exit_after_transverse();
  test_queue_handoff_interrupt();
  test_sampling_during_blocked_main();
  test_direction_after_unconfirmed_middle();
  test_single_outer_flash_search_direction();
  test_lone_inner_uses_encoder_bounded_probe();
  test_three_black_cancels_corner();
  test_patterns_and_narrow_windows();
  test_corner_edge_chatter();
  for(smooth=0;smooth<=1;++smooth) for(forward=0;forward<=1;++forward)
  {
    reset((uint8_t)forward,(uint8_t)smooth); hold(5,300); sample(0,70,3000);
    assert(!output.valid && telemetry.mode==DRIVE_BASE_SPEED && !brakes && LineRecovery_IsSearching());
    hold(0,90000); assert_search(); assert(attacks==0 && !buzzer); /* Persistent search remains silent for 90 seconds. */
    hold(2,10000); assert(output.valid && output.left_cps==-2200 && output.right_cps==2200);
    hold(8,10000); assert(output.valid && output.left_cps==2200 && output.right_cps==-2200);
    hold(3,1000); assert(output.valid && output.left_cps>=0 && output.right_cps>=0 && output.left_cps+output.right_cps>0);
    sample(5,10,3000); assert(telemetry.mode==DRIVE_BASE_SPEED);
    hold(0,200); assert(!output.valid && LineRecovery_IsSearching()); /* False contact resumes, never latches stop. */
    hold(5,180); assert(output.valid && output.left_cps>0 && !BuzzerPhrase400_IsPlaying() && !buzzer);
    hold(2,2000); assert(output.valid && output.left_cps==-2200 && output.right_cps==2200);
    hold(5,750); assert(output.left_cps>2400 && !BuzzerPhrase400_IsPlaying());
    hold(5,500); assert(output.left_cps==2700 && output.right_cps==2700);
    sample(0,10,3000); hold(0,160); assert_search();
    /* Same cancellation hook that main calls on remote STOP / mode handoff. */
    line_tracking_reset(); assert(telemetry.mode==DRIVE_BASE_STOPPED && !BuzzerPhrase400_IsPlaying() && !buzzer);
    for(i=0;i<100;++i) sample(5,10,0);
    assert(output.valid && !output.left_cps && !BuzzerPhrase400_IsPlaying());

    /* A lone inner observation starts an encoder-bounded probe. With no
       simulated wheel motion, elapsed time alone must not reverse it. */
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
  reset(0,0); manual_phrase_test=1; BuzzerPhrase400_Start(2); sample(0,0,3000); assert(buzzer==GPIO_PIN_SET);
  hold(0,110); assert(!buzzer); hold(0,30); assert(buzzer);
  hold(0,150); assert(!buzzer); hold(0,10); assert(buzzer);
  hold(0,140); assert(!buzzer); hold(0,60); assert(buzzer);
  hold(0,170); assert(!buzzer); hold(0,12); assert(buzzer);
  hold(0,280); assert(!buzzer); hold(0,568); assert(buzzer && attacks==6);
  /* Normal mode reset must not claim an unrelated manually started phrase. */
  line_tracking_reset(); BuzzerPhrase400_Start(1); line_tracking_reset();
  assert(BuzzerPhrase400_IsPlaying()); BuzzerPhrase400_Stop();
  manual_phrase_test=0;
  reset(1,1); sample(0,10,3000); assert(output.left_cps>0 && !BuzzerPhrase400_IsPlaying());
  tick=UINT32_MAX-500; reset(0,1); hold(0,10000); assert_search();
  hold(5,700); assert(output.left_cps>2400 && !BuzzerPhrase400_IsPlaying());
  printf("PASS CPS=%ld: silent persistent rotation, independent manual phrase, capture/re-loss, reset/STOP, faults, wrap\n",(long)LINE_SEARCH_TARGET_CPS);
  return 0;
}
