#ifndef LINE_SENSOR_SAMPLE_H
#define LINE_SENSOR_SAMPLE_H
#include <stdint.h>
#define LINE_SENSOR_QUEUE_SIZE 256U
typedef struct { uint32_t time_ms; uint8_t mask; } LineSensorSample;
void LineSensorSample_Start(void);
void LineSensorSample_Reset(void);
/* Called only from the HAL millisecond tick; never drives motors or prints. */
void LineSensorSample_Tick(uint32_t now);
uint8_t LineSensorSample_Pop(LineSensorSample *sample);
uint32_t LineSensorSample_Overwritten(void);
#endif
