#ifndef STORAGE_H
#define STORAGE_H
#include "ff.h"
#include <stdint.h>
#define STORAGE_PATH_MAX 240
#define STORAGE_PAGE 8
#define STORAGE_SAVE_SIZE 32768UL

/* total == 0 denotes indeterminate work; (1, 1) completes it successfully. */
typedef void (*StorageProgress)(uint8_t stage, uint16_t completed,
                                uint16_t total);

typedef struct {
  char name[13];
  char label[19];
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
  ST_FINISHED,
  ST_FIND_BACKUP,
  ST_SYNC_BACKUP,
  ST_SYNC_SAVE,
  ST_VALIDATE,
  ST_PREPARE
};
uint8_t storage_mount(StorageProgress progress) BANKED;
/* Enumeration uses save scratch WRAM; finish/discard edits before browsing. */
uint8_t storage_list(const char *path, uint16_t start,
                     StorageProgress progress) BANKED;
void storage_name(uint8_t index, char *name) BANKED;
uint8_t storage_load(const char *path, StorageProgress progress) BANKED;
uint8_t storage_commit(StorageProgress progress) BANKED;
#endif
