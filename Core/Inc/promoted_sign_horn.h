#ifndef Promoted_SIGN_HORN_H
#define Promoted_SIGN_HORN_H
#include "vision_detection.h"
void Promoted_SignHorn_Reset(void);
/* Three fresh consecutive horn frames trigger once; 800ms observed absence rearms. */
uint8_t Promoted_SignHorn_Observe(const VisionDetection *frame, uint32_t now);
#endif
