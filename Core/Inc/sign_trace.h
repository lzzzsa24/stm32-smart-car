#ifndef SIGN_TRACE_H
#define SIGN_TRACE_H
#include <stdint.h>
#include "sign_route.h"
#define SIGN_TRACE_CAPACITY 256U
typedef struct {
  uint32_t time_ms, sequence;
  int32_t yaw_mdeg, travel_mm;
  uint8_t mask, state, fault, online, score;
  int8_t direction, class_id;
} SignTraceRecord;
void SignTrace_Init(void);
void SignTrace_Record(uint32_t now, uint8_t mask, const SignRouteStatus *status);
void SignTrace_Request(uint8_t clear);
void SignTrace_Task(uint8_t stopped);
uint16_t SignTrace_Count(void);
uint8_t SignTrace_Get(uint16_t index, SignTraceRecord *out);
#endif
