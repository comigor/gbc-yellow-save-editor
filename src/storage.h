#ifndef STORAGE_H
#define STORAGE_H
#include "ff.h"
#include <stdint.h>
#define STORAGE_PATH_MAX 240
#define STORAGE_PAGE 8
#define STORAGE_SAVE_SIZE 32768UL

typedef struct {
  char name[13];
  uint8_t directory;
} StorageEntry;
extern StorageEntry storage_entries[STORAGE_PAGE];
extern uint8_t storage_count, storage_more;
extern uint8_t storage_error, storage_stage;
extern char storage_backup[STORAGE_PATH_MAX];

enum {
  STORE_OK,
  STORE_FS,
  STORE_SIZE,
  STORE_CHANGED,
  STORE_VERIFY,
  STORE_FULL,
  STORE_BACKUPS_FULL,
  STORE_PATH,
  STORE_NOT_FAT32
};
enum {
  ST_MOUNT,
  ST_BROWSE,
  ST_LOAD,
  ST_BACKUP,
  ST_VERIFY_BACKUP,
  ST_CHECK_SOURCE,
  ST_WRITE,
  ST_VERIFY_SAVE,
  ST_FINISHED
};
uint8_t storage_mount(void) BANKED;
uint8_t storage_list(const char *path, uint16_t start) BANKED;
uint8_t storage_load(const char *path) BANKED;
uint8_t storage_commit(void (*progress)(uint8_t stage)) BANKED;
#endif
