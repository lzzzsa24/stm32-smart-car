#ifndef Promoted_SIGN_TRACE_H
#define Promoted_SIGN_TRACE_H
#include <stdint.h>
#include "promoted_sign_route.h"
#define Promoted_SIGN_TRACE_CAPACITY 256U
typedef struct {
  uint32_t time_ms, sequence;
  int32_t yaw_mdeg, travel_mm;
  int32_t heading_error_mdeg, arc_peak_mdeg, left_cps, right_cps;
  int32_t exit_heading_peak_mdeg;
  uint8_t mask, state, fault, online, score;
  uint8_t approach_from_pause;
  uint8_t road_reference_valid, exit_reason;
  uint8_t line_action, control_owner, route_active;
  int32_t arc_sweep_mdeg;
  int8_t direction, class_id;
} Promoted_SignTraceRecord;
void Promoted_SignTrace_Init(void);
void Promoted_SignTrace_Record(uint32_t now, uint8_t mask, const Promoted_SignRouteStatus *status,
                      int32_t left_cps, int32_t right_cps, uint8_t line_action,
                      uint8_t control_owner, uint8_t route_active);
void Promoted_SignTrace_Request(uint8_t clear);
void Promoted_SignTrace_Task(uint8_t stopped);
uint16_t Promoted_SignTrace_Count(void);
uint8_t Promoted_SignTrace_Get(uint16_t index, Promoted_SignTraceRecord *out);
#endif
