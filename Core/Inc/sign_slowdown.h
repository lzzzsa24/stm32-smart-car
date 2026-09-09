#ifndef SIGN_SLOWDOWN_H
#define SIGN_SLOWDOWN_H
#include <stdint.h>
#include "vision_detection.h"

#define SIGN_SLOWDOWN_HOLD_MS 1500U
#define SIGN_SLOWDOWN_FRAME_MAX_AGE_MS 350U
#define SIGN_SLOWDOWN_LIMIT_CPS 1200L
#define SIGN_SLOWDOWN_BLACK_LIMIT_CPS 700L
#define SIGN_SLOWDOWN_VISION_LIMIT_CPS 500L
#define SIGN_SLOWDOWN_SEARCH_LIMIT_CPS 500L
#define SIGN_SLOWDOWN_BLACK 1U
#define SIGN_SLOWDOWN_VISION 2U

void SignSlowdown_Reset(void);
void SignSlowdown_AllowPause(uint8_t allowed);
uint8_t SignSlowdown_Paused(uint32_t now);
void SignSlowdown_ObserveBlack(uint32_t sampled_ms);
/* Call only for a complete, validated parser frame in an active sign mode. */
void SignSlowdown_ObserveDetection(const VisionDetection *frame, uint32_t now);
uint8_t SignSlowdown_Reasons(uint32_t now);
int32_t SignSlowdown_ForwardCps(int16_t request);
/* Limit translation and counter-rotation search separately in sign modes. */
int32_t SignSlowdown_TargetLimit(uint8_t reasons, int16_t left_pwm, int16_t right_pwm);
#endif
