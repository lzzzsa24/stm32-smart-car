#include "main.h"
volatile uint32_t uwTick;
uint32_t uwTickFreq = 1U;
void (*test_irq_restore_hook)(void);
void Test_RestoreIrq(uint32_t value)
{
  (void)value;
  if(test_irq_restore_hook) test_irq_restore_hook();
}
