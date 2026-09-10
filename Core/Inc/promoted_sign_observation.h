#ifndef Promoted_SIGN_OBSERVATION_H
#define Promoted_SIGN_OBSERVATION_H
#include <stdint.h>
#include "vision_detection.h"

/* Recognition pause only. No driving-speed cap or PWM conversion. */
void Promoted_SignObservation_Reset(void);
void Promoted_SignObservation_AllowPause(uint8_t allowed);
void Promoted_SignObservation_ObserveDetection(const VisionDetection *frame, uint32_t now);
uint8_t Promoted_SignObservation_Paused(uint32_t now);
/* Hold navigation for the fixed pause, independent of black-line contact. */
uint8_t Promoted_SignObservation_HoldingRoute(uint32_t now);
#endif
