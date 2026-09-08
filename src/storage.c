#include "storage.h"
#include "checksum.h"
#include "save_memory.h"
#include "text.h"
#include <string.h>
#ifdef __SDCC
#pragma bank 2
#endif

StorageEntry storage_entries[STORAGE_PAGE];
uint8_t storage_count, storage_more, storage_error, storage_stage;
char storage_backup[STORAGE_PATH_MAX];
static char loaded_path[STORAGE_PATH_MAX];
static FATFS filesystem;
static FIL source_file, output_file;
static DIR directory;
static FILINFO info;
static uint8_t buffer[256];
static uint32_t original_crc;
static char volume_path[1];

static uint8_t fs_result(FRESULT r) {
  storage_error = (uint8_t)r;
  return r == FR_OK ? STORE_OK : STORE_FS;
}

/* Completion is withheld until close and checksum checks succeed. */
static void report_chunk(StorageProgress progress, uint16_t completed,
                         uint16_t total) {
  if (progress && completed != total)
    progress(storage_stage, completed, total);
}

static uint8_t remount(void) {
  FRESULT r = f_mount(0, volume_path, 0);
  if (r == FR_OK)
    r = f_mount(&filesystem, volume_path, 1);
  if (r != FR_OK)
    return fs_result(r);
  return filesystem.fs_type == FS_FAT32 ? STORE_OK : STORE_NOT_FAT32;
}

uint8_t storage_mount(StorageProgress progress) BANKED {
  uint8_t result;
  storage_stage = ST_MOUNT;
  progress(ST_MOUNT, 0, 0);
  result = remount();
  if (!result)
    progress(ST_MOUNT, 1, 1);
  return result;
}

static uint8_t save_extension(const char *name) {
  const char *p = text_last(name, '.');
  char ext[4];
  uint8_t i;
  if (!p || strlen(p) != 4)
    return 0;
  for (i = 0; i < 3; ++i) {
    ext[i] = p[i + 1];
    if (ext[i] >= 'a' && ext[i] <= 'z')
      ext[i] -= 32;
  }
  ext[3] = 0;
  return !strcmp(ext, "SRM") || !strcmp(ext, "SAV");
}

uint8_t storage_list(const char *path, uint16_t start,
                     StorageProgress progress) BANKED {
  FRESULT r;
  uint16_t skipped = 0, position, visited = 0;
  const char *display_name;
  storage_stage = ST_BROWSE;
  storage_count = storage_more = 0;
  if (progress)
    progress(ST_BROWSE, 0, 0);
  r = f_opendir(&directory, path);
  if (r != FR_OK)
    return fs_result(r);
  for (;;) {
    r = f_readdir(&directory, &info);
    if (r != FR_OK || !info.fname[0])
      break;
    ++visited;
    report_chunk(progress, visited, 0);
    if (info.fname[0] == '.' || (info.fattrib & (AM_HID | AM_SYS)))
      continue;
    if (!(info.fattrib & AM_DIR) && !save_extension(info.fname))
      continue;
    if (skipped < start) {
      ++skipped;
      continue;
    }
    if (storage_count == STORAGE_PAGE) {
      storage_more = 1;
      break;
    }
    strcpy(storage_entries[storage_count].name, info.fname);
    display_name = info.lfname[0] ? info.lfname : info.fname;
    /* Before a save is loaded, bank 2 holds full names for the browser page. */
    for (position = 0;; ++position) {
      save_set(0x2000u + (uint16_t)storage_count * 256u + position,
               display_name[position]);
      if (!display_name[position])
        break;
    }
    for (position = 0; position < 18 && display_name[position]; ++position)
      storage_entries[storage_count].label[position] = display_name[position];
    storage_entries[storage_count].label[position] = 0;
    storage_entries[storage_count].directory = (info.fattrib & AM_DIR) != 0;
    ++storage_count;
  }
  if (r != FR_OK)
    return fs_result(r);
  r = f_closedir(&directory);
  if (r != FR_OK)
    return fs_result(r);
  if (progress)
    progress(ST_BROWSE, 1, 1);
  return STORE_OK;
}

void storage_name(uint8_t index, char *name) BANKED {
  uint16_t position = 0;
  do {
    name[position] = save_get(0x2000u + (uint16_t)index * 256u + position);
  } while (name[position++]);
}

static uint8_t read_block(FIL *fp) {
  UINT count;
  FRESULT r = f_read(fp, buffer, sizeof(buffer), &count);
  if (r != FR_OK)
    return fs_result(r);
  return count == sizeof(buffer) ? STORE_OK : STORE_SIZE;
}

static uint8_t open_save(const char *path) {
  FRESULT r = f_open(&source_file, path, FA_READ);
  if (r != FR_OK)
    return fs_result(r);
  if (f_size(&source_file) != STORAGE_SAVE_SIZE) {
    f_close(&source_file);
    return STORE_SIZE;
  }
  return STORE_OK;
}

static uint8_t stream_crc(const char *path, uint32_t expected,
                          uint8_t compare_memory, StorageProgress progress) {
  uint16_t offset, i;
  uint32_t crc = 0xffffffffUL;
  uint8_t result = open_save(path);
  if (result)
    return result;
  for (offset = 0; offset < 0x8000u; offset += sizeof(buffer)) {
    result = read_block(&source_file);
    if (result)
      return result;
    crc = checksum_update(crc, buffer, sizeof(buffer));
    if (compare_memory && offset >= 0x2000u)
      for (i = 0; i < sizeof(buffer); ++i)
        if (buffer[i] != save_get(offset + i))
          return STORE_VERIFY;
    report_chunk(progress, (uint16_t)(offset + sizeof(buffer)),
                 (uint16_t)STORAGE_SAVE_SIZE);
  }
  result = fs_result(f_close(&source_file));
  if (result)
    return result;
  if ((crc ^ 0xffffffffUL) != expected)
    return STORE_CHANGED;
  if (progress)
    progress(storage_stage, (uint16_t)STORAGE_SAVE_SIZE,
             (uint16_t)STORAGE_SAVE_SIZE);
  return STORE_OK;
}

uint8_t storage_load(const char *path, StorageProgress progress) BANKED {
  uint16_t offset, i;
  uint8_t result;
  storage_stage = ST_LOAD;
  if (strlen(path) >= sizeof(loaded_path))
    return STORE_PATH;
  if (progress)
    progress(ST_LOAD, 0, (uint16_t)STORAGE_SAVE_SIZE);
  result = open_save(path);
  if (result)
    return result;
  original_crc = 0xffffffffUL;
  for (offset = 0; offset < 0x8000u; offset += sizeof(buffer)) {
    result = read_block(&source_file);
    if (result)
      return result;
    original_crc = checksum_update(original_crc, buffer, sizeof(buffer));
    if (offset >= 0x2000u)
      for (i = 0; i < sizeof(buffer); ++i)
        save_set(offset + i, buffer[i]);
    report_chunk(progress, (uint16_t)(offset + sizeof(buffer)),
                 (uint16_t)STORAGE_SAVE_SIZE);
  }
  result = fs_result(f_close(&source_file));
  if (result)
    return result;
  original_crc ^= 0xffffffffUL;
  if (progress)
    progress(ST_LOAD, (uint16_t)STORAGE_SAVE_SIZE, (uint16_t)STORAGE_SAVE_SIZE);
  strcpy(loaded_path, path);
  return STORE_OK;
}

static uint8_t create_backup(StorageProgress progress) {
  char *name;
  uint16_t n, value;
  int8_t digit;
  FRESULT r;
  strcpy(storage_backup, loaded_path);
  name = text_last(storage_backup, '/');
  name = name ? name + 1 : storage_backup;
  if ((uint16_t)(name - storage_backup) + 13 >= sizeof(storage_backup))
    return STORE_PATH;
  for (n = 1; n <= 999; ++n) {
    strcpy(name, "PK000000.BAK");
    value = n;
    for (digit = 7; digit >= 2; --digit) {
      name[(uint8_t)digit] = '0' + value % 10;
      value /= 10;
    }
    r = f_open(&output_file, storage_backup, FA_CREATE_NEW | FA_WRITE);
    if (r == FR_OK) {
      if (progress)
        progress(ST_FIND_BACKUP, 1, 1);
      return STORE_OK;
    }
    if (r != FR_EXIST)
      return fs_result(r);
    report_chunk(progress, n, 0);
  }
  return STORE_BACKUPS_FULL;
}

uint8_t storage_commit(StorageProgress progress) BANKED {
  uint16_t offset, i;
  UINT count;
  FRESULT r;
  uint8_t result;
  uint32_t crc, expected;
  storage_stage = ST_CHECK_SOURCE;
  if (progress)
    progress(ST_CHECK_SOURCE, 0, (uint16_t)STORAGE_SAVE_SIZE);
  result = remount();
  if (result)
    return result;
  result = stream_crc(loaded_path, original_crc, 0, progress);
  if (result)
    return result;
  storage_stage = ST_FIND_BACKUP;
  if (progress)
    progress(ST_FIND_BACKUP, 0, 0);
  result = open_save(loaded_path);
  if (result)
    return result;
  result = create_backup(progress);
  if (result)
    return result;
  storage_stage = ST_BACKUP;
  if (progress)
    progress(ST_BACKUP, 0, (uint16_t)STORAGE_SAVE_SIZE);
  crc = expected = 0xffffffffUL;
  for (offset = 0; offset < 0x8000u; offset += sizeof(buffer)) {
    result = read_block(&source_file);
    if (result)
      return result;
    crc = checksum_update(crc, buffer, sizeof(buffer));
    r = f_write(&output_file, buffer, sizeof(buffer), &count);
    if (r != FR_OK)
      return fs_result(r);
    if (count != sizeof(buffer))
      return STORE_FULL;
    if (offset >= 0x2000u)
      for (i = 0; i < sizeof(buffer); ++i)
        buffer[i] = save_get(offset + i);
    expected = checksum_update(expected, buffer, sizeof(buffer));
    report_chunk(progress, (uint16_t)(offset + sizeof(buffer)),
                 (uint16_t)STORAGE_SAVE_SIZE);
  }
  result = fs_result(f_close(&source_file));
  if (result)
    return result;
  storage_stage = ST_SYNC_BACKUP;
  if (progress)
    progress(ST_SYNC_BACKUP, 0, 0);
  result = fs_result(f_close(&output_file));
  if (result)
    return result;
  if ((crc ^ 0xffffffffUL) != original_crc)
    return STORE_CHANGED;
  if (progress) {
    progress(ST_SYNC_BACKUP, 1, 1);
    storage_stage = ST_BACKUP;
    progress(ST_BACKUP, (uint16_t)STORAGE_SAVE_SIZE,
             (uint16_t)STORAGE_SAVE_SIZE);
  }
  expected ^= 0xffffffffUL;
  storage_stage = ST_VERIFY_BACKUP;
  if (progress)
    progress(ST_VERIFY_BACKUP, 0, (uint16_t)STORAGE_SAVE_SIZE);
  result = remount();
  if (result)
    return result;
  result = stream_crc(storage_backup, original_crc, 0, progress);
  if (result)
    return result;
  storage_stage = ST_CHECK_SOURCE;
  if (progress)
    progress(ST_CHECK_SOURCE, 0, (uint16_t)STORAGE_SAVE_SIZE);
  result = remount();
  if (result)
    return result;
  result = stream_crc(loaded_path, original_crc, 0, progress);
  if (result)
    return result;
  storage_stage = ST_WRITE;
  if (progress)
    progress(ST_WRITE, 0, 24576);
  r = f_open(&output_file, loaded_path, FA_OPEN_EXISTING | FA_WRITE);
  if (r != FR_OK)
    return fs_result(r);
  if (f_size(&output_file) != STORAGE_SAVE_SIZE)
    return STORE_CHANGED;
  r = f_lseek(&output_file, 0x2000UL);
  if (r != FR_OK)
    return fs_result(r);
  for (offset = 0x2000u; offset < 0x8000u; offset += sizeof(buffer)) {
    for (i = 0; i < sizeof(buffer); ++i)
      buffer[i] = save_get(offset + i);
    r = f_write(&output_file, buffer, sizeof(buffer), &count);
    if (r != FR_OK)
      return fs_result(r);
    if (count != sizeof(buffer))
      return STORE_FULL;
    report_chunk(progress, (uint16_t)(offset + sizeof(buffer) - 0x2000u),
                 24576);
  }
  storage_stage = ST_SYNC_SAVE;
  if (progress)
    progress(ST_SYNC_SAVE, 0, 0);
  result = fs_result(f_close(&output_file));
  if (result)
    return result;
  if (progress) {
    progress(ST_SYNC_SAVE, 1, 1);
    storage_stage = ST_WRITE;
    progress(ST_WRITE, 24576, 24576);
  }
  storage_stage = ST_VERIFY_SAVE;
  if (progress)
    progress(ST_VERIFY_SAVE, 0, (uint16_t)STORAGE_SAVE_SIZE);
  result = remount();
  if (result)
    return result;
  result = stream_crc(loaded_path, expected, 1, progress);
  if (result)
    return result;
  original_crc = expected;
  storage_stage = ST_FINISHED;
  if (progress)
    progress(ST_FINISHED, 1, 1);
  return STORE_OK;
}
