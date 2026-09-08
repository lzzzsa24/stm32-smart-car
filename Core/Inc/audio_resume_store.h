#ifndef AUDIO_RESUME_STORE_H
#define AUDIO_RESUME_STORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* STM32F103ZE second-to-last 2 KiB page.  The final page at 0x0807F800
 * remains untouched for the existing motor/turn calibration data. */
#ifndef AUDIO_RESUME_STORE_FLASH_ADDRESS
#define AUDIO_RESUME_STORE_FLASH_ADDRESS  0x0807F000UL
#endif

#ifndef AUDIO_RESUME_STORE_FLASH_PAGE_SIZE
#define AUDIO_RESUME_STORE_FLASH_PAGE_SIZE 2048U
#endif

typedef enum
{
  AUDIO_RESUME_STORE_IDLE = 0U,
  AUDIO_RESUME_STORE_SAVED,
  AUDIO_RESUME_STORE_DEFERRED,
  AUDIO_RESUME_STORE_ERROR
} AudioResumeStoreResult;

void AudioResumeStore_Init(void);
uint16_t AudioResumeStore_GetTrack(void);
uint8_t AudioResumeStore_RequestTrack(uint16_t track_number);

/* Appends without erasing whenever a free slot exists.  A full-page erase is
 * performed only when allow_page_erase is nonzero (the app passes true only
 * while the vehicle is in STOP). */
AudioResumeStoreResult AudioResumeStore_Task(uint8_t allow_page_erase);

uint8_t AudioResumeStore_HasPending(void);
uint8_t AudioResumeStore_HasError(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_RESUME_STORE_H */
