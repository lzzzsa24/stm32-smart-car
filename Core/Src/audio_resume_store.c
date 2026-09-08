#include "main.h"
#include "audio_resume_store.h"

#include <stddef.h>

#define AUDIO_RESUME_MAGIC       0x41554431UL /* "AUD1" */
#define AUDIO_RESUME_CHECK_SEED  0xC39A5A6DUL

typedef struct
{
  uint32_t magic;
  uint32_t sequence;
  uint32_t track_and_inverse;
  uint32_t checksum;
} AudioResumeRecord;

#define AUDIO_RESUME_RECORD_COUNT \
  (AUDIO_RESUME_STORE_FLASH_PAGE_SIZE / sizeof(AudioResumeRecord))

static uint16_t requested_track;
static uint16_t committed_track;
static uint32_t next_sequence;
static uint8_t save_pending;
static uint8_t store_error;

static const AudioResumeRecord *record_at(uint32_t index)
{
  return (const AudioResumeRecord *)(uintptr_t)
      (AUDIO_RESUME_STORE_FLASH_ADDRESS + index * sizeof(AudioResumeRecord));
}

static uint32_t record_checksum(uint32_t sequence, uint32_t track_word)
{
  return AUDIO_RESUME_MAGIC ^ sequence ^ track_word ^
         AUDIO_RESUME_CHECK_SEED;
}

static uint8_t record_is_empty(const AudioResumeRecord *record)
{
  return (record->magic == 0xFFFFFFFFUL &&
          record->sequence == 0xFFFFFFFFUL &&
          record->track_and_inverse == 0xFFFFFFFFUL &&
          record->checksum == 0xFFFFFFFFUL) ? 1U : 0U;
}

static uint8_t record_is_valid(const AudioResumeRecord *record)
{
  uint16_t track = (uint16_t)(record->track_and_inverse & 0xFFFFU);
  uint16_t inverse = (uint16_t)(record->track_and_inverse >> 16U);

  return (record->magic == AUDIO_RESUME_MAGIC && track != 0U &&
          (uint16_t)(track ^ inverse) == 0xFFFFU &&
          record->checksum == record_checksum(record->sequence,
                                               record->track_and_inverse)) ?
         1U : 0U;
}

static uint8_t sequence_is_newer(uint32_t candidate, uint32_t current)
{
  return (int32_t)(candidate - current) > 0 ? 1U : 0U;
}

static uint32_t increment_sequence(uint32_t value)
{
  ++value;
  if (value == 0U || value == 0xFFFFFFFFUL)
  {
    value = 1U;
  }
  return value;
}

static uint32_t find_empty_slot(void)
{
  uint32_t index;

  for (index = 0U; index < AUDIO_RESUME_RECORD_COUNT; ++index)
  {
    if (record_is_empty(record_at(index)) != 0U)
    {
      return index;
    }
  }
  return AUDIO_RESUME_RECORD_COUNT;
}

static uint8_t erase_page(void)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t page_error = 0U;

  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.PageAddress = AUDIO_RESUME_STORE_FLASH_ADDRESS;
  erase.NbPages = 1U;
  return HAL_FLASHEx_Erase(&erase, &page_error) == HAL_OK ? 1U : 0U;
}

static uint8_t program_record(uint32_t slot,
                              const AudioResumeRecord *record)
{
  uintptr_t address = AUDIO_RESUME_STORE_FLASH_ADDRESS +
                      slot * sizeof(AudioResumeRecord);
  HAL_StatusTypeDef status;

  /* Commit magic last, so a power loss cannot make a partial record valid. */
  status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                             address + offsetof(AudioResumeRecord, sequence),
                             record->sequence);
  if (status == HAL_OK)
  {
    status = HAL_FLASH_Program(
        FLASH_TYPEPROGRAM_WORD,
        address + offsetof(AudioResumeRecord, track_and_inverse),
        record->track_and_inverse);
  }
  if (status == HAL_OK)
  {
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                               address + offsetof(AudioResumeRecord, checksum),
                               record->checksum);
  }
  if (status == HAL_OK)
  {
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                               address + offsetof(AudioResumeRecord, magic),
                               record->magic);
  }
  return status == HAL_OK && record_is_valid(record_at(slot)) != 0U ? 1U : 0U;
}

void AudioResumeStore_Init(void)
{
  uint8_t found = 0U;
  uint32_t newest_sequence = 0U;
  uint16_t newest_track = 1U;
  uint32_t index;

  for (index = 0U; index < AUDIO_RESUME_RECORD_COUNT; ++index)
  {
    const AudioResumeRecord *record = record_at(index);
    if (record_is_valid(record) != 0U &&
        (found == 0U ||
         sequence_is_newer(record->sequence, newest_sequence) != 0U))
    {
      found = 1U;
      newest_sequence = record->sequence;
      newest_track = (uint16_t)(record->track_and_inverse & 0xFFFFU);
    }
  }

  requested_track = newest_track;
  committed_track = newest_track;
  next_sequence = found != 0U ? increment_sequence(newest_sequence) : 1U;
  save_pending = 0U;
  store_error = 0U;
}

uint16_t AudioResumeStore_GetTrack(void)
{
  return requested_track;
}

uint8_t AudioResumeStore_RequestTrack(uint16_t track_number)
{
  if (track_number == 0U)
  {
    return 0U;
  }
  requested_track = track_number;
  save_pending = track_number != committed_track ? 1U : 0U;
  store_error = 0U;
  return 1U;
}

AudioResumeStoreResult AudioResumeStore_Task(uint8_t allow_page_erase)
{
  AudioResumeRecord record;
  uint32_t slot;
  uint16_t inverse;
  uint8_t success;

  if (save_pending == 0U)
  {
    return AUDIO_RESUME_STORE_IDLE;
  }

  slot = find_empty_slot();
  if (slot >= AUDIO_RESUME_RECORD_COUNT && allow_page_erase == 0U)
  {
    return AUDIO_RESUME_STORE_DEFERRED;
  }

  record.magic = AUDIO_RESUME_MAGIC;
  record.sequence = next_sequence;
  inverse = (uint16_t)~requested_track;
  record.track_and_inverse = (uint32_t)requested_track |
                             ((uint32_t)inverse << 16U);
  record.checksum = record_checksum(record.sequence,
                                    record.track_and_inverse);

  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR |
                         FLASH_FLAG_WRPERR);
  if (slot >= AUDIO_RESUME_RECORD_COUNT)
  {
    success = erase_page();
    slot = 0U;
  }
  else
  {
    success = 1U;
  }
  if (success != 0U)
  {
    success = program_record(slot, &record);
  }
  HAL_FLASH_Lock();

  if (success == 0U)
  {
    save_pending = 0U;
    store_error = 1U;
    return AUDIO_RESUME_STORE_ERROR;
  }

  committed_track = requested_track;
  next_sequence = increment_sequence(next_sequence);
  save_pending = 0U;
  return AUDIO_RESUME_STORE_SAVED;
}

uint8_t AudioResumeStore_HasPending(void)
{
  return save_pending;
}

uint8_t AudioResumeStore_HasError(void)
{
  return store_error;
}
