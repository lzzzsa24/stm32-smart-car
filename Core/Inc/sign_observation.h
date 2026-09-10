#ifndef SIGN_OBSERVATION_H
#define SIGN_OBSERVATION_H
#include <stdint.h>
#include "vision_detection.h"

#define SIGN_OBSERVATION_MODE3_SCORE_MINIMUM 22U
#define SIGN_OBSERVATION_MODE4_SCORE_MINIMUM 26U

/* Recognition pause only. No driving-speed cap or PWM conversion. */
void SignObservation_Reset(void);
void SignObservation_AllowPause(uint8_t allowed);
void SignObservation_ObserveDetection(const VisionDetection *frame, uint32_t now,
                                     uint8_t minimum_score);
uint8_t SignObservation_Paused(uint32_t now);
/* Hold navigation for the fixed pause, independent of black-line contact. */
uint8_t SignObservation_HoldingRoute(uint32_t now);
#endif
