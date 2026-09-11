#ifndef LINE_FAULT_LOG_H
#define LINE_FAULT_LOG_H
#include <stdint.h>

#define LINE_FAULT_LOG_CAPACITY 32U
#define LINE_SEARCH_LOG_CAPACITY 16U
typedef enum { LINE_SEARCH_DEFAULT=0, LINE_SEARCH_HINT=1,
               LINE_SEARCH_REJOIN=2, LINE_SEARCH_CORNER=3,
               LINE_SEARCH_CORRECTION=4, LINE_SEARCH_WAIT_RECOVERY=5,
               LINE_SEARCH_CROSS_HINT=6,
               LINE_SEARCH_INNER_PROBE=7 } LineSearchSource;
typedef struct
{
  uint32_t time_ms, edge_age_ms, wide_age_ms, queue_overwritten;
  uint8_t edge_mask, wide_mask;
  int8_t chosen_side, hint;
  LineSearchSource source;
  uint8_t pause_reason, drive_fault, bypass_fault;
  uint8_t gyro_fault, imu_fault;
  /* Actual accepted hint origin, including inner probes and adjacent triples.
     edge_mask retains its original narrow-outer-only meaning. */
  uint8_t hint_mask;
  uint32_t hint_age_ms;
} LineSearchRecord;
/* Decisions persist across mode reset/STOP, independently of motor faults. */
void LineFaultLog_RecordSearch(const LineSearchRecord *record);
uint32_t LineFaultLog_SearchCount(void);
uint8_t LineFaultLog_GetSearch(uint32_t oldest_index, LineSearchRecord *record);
typedef struct
{
  uint32_t sequence, first_ms, last_ms, occurrences, elapsed_ms;
  uint8_t stall_mask, direction_mask, signal_mask, sensor_mask;
  uint8_t recovery_state, degraded_mask;
  uint16_t battery_mv;
  int32_t requested[4], controlled[4], measured[4], delta[4];
  int16_t pwm[4];
  uint32_t illegal_delta[4], no_motion_ms[4];
} LineFaultRecord;

/* Fixed-size RAM history, retained by STOP/mode change/ClearFault, not reboot. */
void LineFaultLog_Init(void);
void LineFaultLog_Record(const LineFaultRecord *record);
uint32_t LineFaultLog_Count(void);
uint32_t LineFaultLog_Overwritten(void);
uint8_t LineFaultLog_Get(uint32_t oldest_index, LineFaultRecord *record);
void LineFaultLog_RequestDump(void);
/* One short line per call, only while operator STOP is active. */
void LineFaultLog_Task(uint8_t operator_stopped);
#endif
