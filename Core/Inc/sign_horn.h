#ifndef SIGN_HORN_H
#define SIGN_HORN_H
#include "vision_detection.h"
void SignHorn_Reset(void);
/* Three fresh consecutive horn frames trigger once; 800ms observed absence rearms. */
uint8_t SignHorn_Observe(const VisionDetection *frame, uint32_t now);
#endif
