#ifndef SIGN_OBSERVATION_H
#define SIGN_OBSERVATION_H
#include <stdint.h>
#include "vision_detection.h"

/* Recognition pause only. No driving-speed cap or PWM conversion. */
void SignObservation_Reset(void);
void SignObservation_AllowPause(uint8_t allowed);
void SignObservation_ObserveDetection(const VisionDetection *frame, uint32_t now);
uint8_t SignObservation_Paused(uint32_t now);
#endif
