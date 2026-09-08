#include "diskio.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

FILE *test_disk;
uint8_t test_stage;
const char *test_fault = "none";
unsigned test_writes;
static uint8_t ready;

DSTATUS disk_initialize(FF_BYTE drive) {
  ready = drive == 0;
  return ready ? 0 : STA_NOINIT;
}
DSTATUS disk_status(FF_BYTE drive) {
  return drive == 0 && ready ? 0 : STA_NOINIT;
}
DRESULT disk_read(FF_BYTE drive, FF_BYTE *data, LBA_t sector, UINT count) {
  if (drive || !ready)
    return RES_NOTRDY;
  assert(fseek(test_disk, (long)sector * 512, SEEK_SET) == 0);
  if (fread(data, 512, count, test_disk) != count)
    return RES_ERROR;
  if (!strcmp(test_fault, "backup-corrupt") && test_stage == 4 && count &&
      data[0] == 3 && data[1] == 20 && data[2] == 37)
    data[0] ^= 1;
  return RES_OK;
}
DRESULT disk_write(FF_BYTE drive, const FF_BYTE *data, LBA_t sector,
                   UINT count) {
  if (drive || !ready)
    return RES_NOTRDY;
  if ((!strcmp(test_fault, "backup-write") && test_stage == 3) ||
      (!strcmp(test_fault, "save-write") && test_stage == 6))
    return RES_ERROR;
  assert(fseek(test_disk, (long)sector * 512, SEEK_SET) == 0);
  if (fwrite(data, 512, count, test_disk) != count)
    return RES_ERROR;
  fflush(test_disk);
  test_writes += count;
  return RES_OK;
}
DRESULT disk_ioctl(FF_BYTE drive, FF_BYTE command, void *data) {
  (void)data;
  return drive == 0 && command == CTRL_SYNC && fflush(test_disk) == 0
             ? RES_OK
             : RES_ERROR;
}
