#include <stddef.h>
#include <string.h>
#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/platform.h"
#include "config_store.h"

#define FLASH_BYTES (2u * 1024u * 1024u)
#define SLOT_BYTES 4096u
#define RESERVED_BYTES (2u * SLOT_BYTES)
#define SLOT0_OFFSET (FLASH_BYTES - RESERVED_BYTES)
#define SLOT1_OFFSET (FLASH_BYTES - SLOT_BYTES)
#define SLOT_MAGIC 0x55324a43u
#define PROGRAM_BYTES 256u

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint32_t generation;
  uint32_t payload_size;
  uint32_t crc;
  uint8_t payload[CONFIG_WIRE_SIZE];
} slot_record_t;

typedef struct {
  uint32_t offset;
  uint8_t image[SLOT_BYTES] __attribute__((aligned(4)));
  size_t program_size;
} write_job_t;

static int active_slot = -1;
static uint32_t active_generation;
static app_config_t load_first, load_second, verify_config;
static write_job_t write_job;
static uint8_t write_payload[CONFIG_WIRE_SIZE];
static slot_record_t write_record;

static const slot_record_t *slot_at(unsigned index) {
  uint32_t offset = index == 0 ? SLOT0_OFFSET : SLOT1_OFFSET;
  return (const slot_record_t *)(XIP_BASE + offset);
}

static bool read_slot(unsigned index, app_config_t *config, uint32_t *generation) {
  const slot_record_t *record = slot_at(index);
  if (record->magic != SLOT_MAGIC || record->payload_size != CONFIG_WIRE_SIZE) return false;
  if (config_crc32(record->payload, CONFIG_WIRE_SIZE) != record->crc) return false;
  if (!config_decode(record->payload, CONFIG_WIRE_SIZE, config)) return false;
  *generation = record->generation;
  return true;
}

void config_store_init(app_config_t *config) {
  uint32_t gen0 = 0, gen1 = 0;
  bool valid0 = read_slot(0, &load_first, &gen0);
  bool valid1 = read_slot(1, &load_second, &gen1);
  if (!valid0 && !valid1) {
    active_slot = -1;
    active_generation = 0;
    return;
  }
  if (valid1 && (!valid0 || (int32_t)(gen1 - gen0) > 0)) {
    *config = load_second;
    active_slot = 1;
    active_generation = gen1;
  } else {
    *config = load_first;
    active_slot = 0;
    active_generation = gen0;
  }
}

static void __no_inline_not_in_flash_func(flash_write_job)(void *opaque) {
  const write_job_t *job = (const write_job_t *)opaque;
  flash_range_erase(job->offset, SLOT_BYTES);
  flash_range_program(job->offset, job->image, job->program_size);
}

bool config_store_save(const app_config_t *config) {
  if (!config_validate(config)) return false;
  memset(&write_job, 0xff, sizeof(write_job));
  write_job.offset = active_slot == 0 ? SLOT1_OFFSET : SLOT0_OFFSET;
  config_encode(config, write_payload);
  write_record.magic = SLOT_MAGIC;
  write_record.generation = active_generation + 1;
  write_record.payload_size = CONFIG_WIRE_SIZE;
  write_record.crc = config_crc32(write_payload, sizeof(write_payload));
  memcpy(write_record.payload, write_payload, sizeof(write_payload));
  memcpy(write_job.image, &write_record, sizeof(write_record));
  write_job.program_size = (sizeof(write_record) + PROGRAM_BYTES - 1u) & ~(PROGRAM_BYTES - 1u);

  if (flash_safe_execute(flash_write_job, &write_job, 1000) != PICO_OK) return false;

  uint32_t generation;
  unsigned target = write_job.offset == SLOT0_OFFSET ? 0u : 1u;
  if (!read_slot(target, &verify_config, &generation) || generation != write_record.generation) return false;
  active_slot = (int)target;
  active_generation = generation;
  return true;
}
