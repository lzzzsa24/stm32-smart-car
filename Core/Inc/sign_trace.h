#ifndef SIGN_TRACE_H
#define SIGN_TRACE_H
#include <stdint.h>
#include "sign_route.h"
#define SIGN_TRACE_CAPACITY 256U
typedef struct {
  uint32_t time_ms, sequence;
  int32_t yaw_mdeg, travel_mm;
  int32_t heading_error_mdeg, arc_peak_mdeg, left_cps, right_cps;
  uint8_t mask, state, fault, online, score;
  uint8_t approach_from_pause;
  int8_t direction, class_id;
} SignTraceRecord;
void SignTrace_Init(void);
void SignTrace_Record(uint32_t now, uint8_t mask, const SignRouteStatus *status,
                      int32_t left_cps, int32_t right_cps);
void SignTrace_Request(uint8_t clear);
void SignTrace_Task(uint8_t stopped);
uint16_t SignTrace_Count(void);
uint8_t SignTrace_Get(uint16_t index, SignTraceRecord *out);
#endif
