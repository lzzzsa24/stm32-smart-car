#ifndef LINE_WAIT_GUARD_H
#define LINE_WAIT_GUARD_H
#include <stdint.h>
#define LINE_WAIT_LIMIT_MS 800U
#define LINE_WAIT_RECOVERY_MS 1200U
typedef struct { uint32_t since_ms; uint8_t waiting, recovering; } LineWaitGuard;
typedef enum { LINE_WAIT_NONE, LINE_WAIT_BEGIN_RECOVERY,
               LINE_WAIT_RECOVERING, LINE_WAIT_END_RECOVERY } LineWaitAction;
void LineWaitGuard_Reset(LineWaitGuard *guard);
/* Call before any KEY1/KEY2 early return. A changed stop reason cannot restart
   the deadline; explicit mode/STOP cancellation must reset the guard. */
LineWaitAction LineWaitGuard_Update(LineWaitGuard *guard, uint8_t enabled,
                                   uint8_t paused, uint32_t now);
/* Only the application may grant this owner after cancelling the old owner. */
void LineWaitGuard_Drive(int8_t side);
#endif
