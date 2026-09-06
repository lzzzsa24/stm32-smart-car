#include "line_wait_guard.h"
#include "drive_base.h"
#include "line_search_model.h"

void LineWaitGuard_Reset(LineWaitGuard *g)
{ g->waiting = g->recovering = 0U; g->since_ms = 0U; }
LineWaitAction LineWaitGuard_Update(LineWaitGuard *g, uint8_t enabled,
                                   uint8_t paused, uint32_t now)
{
  if (!enabled) { LineWaitGuard_Reset(g); return LINE_WAIT_NONE; }
  if (g->recovering)
  {
    if (now - g->since_ms < LINE_WAIT_RECOVERY_MS) return LINE_WAIT_RECOVERING;
    g->recovering = 0U; g->waiting = 1U; g->since_ms = now;
    return LINE_WAIT_END_RECOVERY;
  }
  if (!paused) { g->waiting = 0U; return LINE_WAIT_NONE; }
  if (!g->waiting) { g->waiting = 1U; g->since_ms = now; }
  if (now - g->since_ms < LINE_WAIT_LIMIT_MS) return LINE_WAIT_NONE;
  g->waiting = 0U; g->recovering = 1U; g->since_ms = now;
  return LINE_WAIT_BEGIN_RECOVERY;
}
void LineWaitGuard_Drive(int8_t side)
{
  int32_t left = side > 0 ? LINE_SEARCH_TARGET_CPS : -LINE_SEARCH_TARGET_CPS;
  DriveBase_SetLineFaultObservation(1U, 0U, 255U);
  DriveBase_PrepareLineTurnAssist(left, -left);
  DriveBase_SetWheelCps(left, left, -left, -left);
}
