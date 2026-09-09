#include "angle_mode_example.h"
#include "gyro_turn.h"

static AngleExampleState phase;
static int32_t result_mdeg;

uint8_t AngleModeExample_Start(int32_t relative_mdeg, int32_t cps)
{
  if (phase == ANGLE_EXAMPLE_RUNNING || phase == ANGLE_EXAMPLE_FAULT) return 0;
  /* Rejection is not completion and is never retried here automatically. */
  if (!GyroTurn_Start(relative_mdeg, cps)) return 0;
  result_mdeg = 0;
  phase = ANGLE_EXAMPLE_RUNNING;
  return 1;
}

void AngleModeExample_Task(void)
{
  GyroTurnState turn;
  if (phase != ANGLE_EXAMPLE_RUNNING) return;
  GyroTurn_Task();
  turn = GyroTurn_GetState();
  if (turn == GYRO_TURN_DONE)
  {
    result_mdeg = GyroTurn_GetAchievedAngleMdeg();
    GyroTurn_Stop();
    phase = ANGLE_EXAMPLE_DONE;
    /* The caller decides the next explicit action; no automatic restart. */
  }
  else if (turn == GYRO_TURN_FAULT)
  {
    result_mdeg = GyroTurn_GetAchievedAngleMdeg();
    GyroTurn_Stop(); /* retains diagnostic fault */
    phase = ANGLE_EXAMPLE_FAULT;
  }
  else if (turn == GYRO_TURN_IDLE)
  {
    phase = ANGLE_EXAMPLE_IDLE; /* cancellation, never report success */
  }
}

void AngleModeExample_Exit(void)
{
  /* Called before a replacement mode starts, not after it has taken motors. */
  if (phase == ANGLE_EXAMPLE_RUNNING) GyroTurn_Stop();
  phase = ANGLE_EXAMPLE_IDLE;
}
AngleExampleState AngleModeExample_GetState(void) { return phase; }
int32_t AngleModeExample_GetAngleMdeg(void) { return result_mdeg; }
