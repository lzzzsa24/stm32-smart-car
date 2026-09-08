#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "audio_resume_store.h"

USART_TypeDef fake_uart4;
GPIO_TypeDef fake_gpioc;
uint32_t fake_hal_tick;
uint32_t fake_audio_flash_words[512];

static uint32_t erase_count;

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config)
{
  (void)port;
  (void)config;
}

uint32_t HAL_RCC_GetPCLK1Freq(void)
{
  return 36000000UL;
}

uint32_t HAL_GetTick(void)
{
  return fake_hal_tick;
}

void HAL_FLASH_Unlock(void)
{
}

void HAL_FLASH_Lock(void)
{
}

HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *erase,
                                    uint32_t *page_error)
{
  if (erase == 0 || erase->PageAddress !=
      (uintptr_t)fake_audio_flash_words || erase->NbPages != 1U)
  {
    return HAL_ERROR;
  }
  memset(fake_audio_flash_words, 0xFF, sizeof(fake_audio_flash_words));
  if (page_error != 0)
  {
    *page_error = 0xFFFFFFFFUL;
  }
  ++erase_count;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type,
                                    uintptr_t address,
                                    uint64_t data)
{
  uintptr_t base = (uintptr_t)fake_audio_flash_words;
  uintptr_t limit = base + sizeof(fake_audio_flash_words);
  uint32_t value = (uint32_t)data;
  uint32_t *target;

  if (type != FLASH_TYPEPROGRAM_WORD || address < base ||
      address + sizeof(uint32_t) > limit ||
      ((address - base) % sizeof(uint32_t)) != 0U)
  {
    return HAL_ERROR;
  }
  target = (uint32_t *)address;
  if ((*target & value) != value)
  {
    return HAL_ERROR;
  }
  *target &= value;
  return HAL_OK;
}

int main(void)
{
  uint32_t index;

  memset(fake_audio_flash_words, 0xFF, sizeof(fake_audio_flash_words));
  erase_count = 0U;
  AudioResumeStore_Init();
  assert(AudioResumeStore_GetTrack() == 1U);
  assert(AudioResumeStore_HasPending() == 0U);
  assert(AudioResumeStore_RequestTrack(0U) == 0U);

  assert(AudioResumeStore_RequestTrack(7U) != 0U);
  assert(AudioResumeStore_Task(0U) == AUDIO_RESUME_STORE_SAVED);
  assert(AudioResumeStore_HasError() == 0U);
  AudioResumeStore_Init();
  assert(AudioResumeStore_GetTrack() == 7U);

  /* Fill all remaining append-only records without a page erase. */
  for (index = 0U; index < 127U; ++index)
  {
    uint16_t track = (index & 1U) != 0U ? 8U : 9U;
    assert(AudioResumeStore_RequestTrack(track) != 0U);
    assert(AudioResumeStore_Task(0U) == AUDIO_RESUME_STORE_SAVED);
  }
  assert(erase_count == 0U);

  assert(AudioResumeStore_RequestTrack(10U) != 0U);
  assert(AudioResumeStore_Task(0U) == AUDIO_RESUME_STORE_DEFERRED);
  assert(AudioResumeStore_HasPending() != 0U);
  assert(AudioResumeStore_Task(1U) == AUDIO_RESUME_STORE_SAVED);
  assert(erase_count == 1U);
  AudioResumeStore_Init();
  assert(AudioResumeStore_GetTrack() == 10U);

  puts("audio resume append log, reload, full-page defer and safe erase passed");
  return 0;
}
