#include "browser.h"
#include "ui.h"
#include "x7_io.h"
#include <gb/gb.h>
#include <gbdk/console.h>
#include <stdio.h>

void show_storage_error(uint8_t result) {
  ui_page("SD operation failed");
  gotoxy(0, 3);
  printf("Error %u FS %u SD %u", (unsigned int)result,
         (unsigned int)storage_error, (unsigned int)x7_error);
  gotoxy(0, 5);
  switch (result) {
  case STORE_SIZE:
    printf("Expected exactly\n32768-byte save.");
    break;
  case STORE_CHANGED:
    printf("Source changed or\nreadback mismatch.");
    break;
  case STORE_VERIFY:
    printf("Readback differs.\nDo not use this save.");
    break;
  case STORE_FULL:
    printf("Card full. Backup\nmay be incomplete.");
    break;
  case STORE_BACKUPS_FULL:
    printf("Backup names full.\nArchive old backups.");
    break;
  case STORE_NOT_FAT32:
    printf("Requires FAT32.");
    break;
  case STORE_PATH:
    printf("Path too long.");
    break;
  default:
    printf("Check card and X7.");
    break;
  }
  ui_line(10, "If write began:");
  ui_line(11, "recover from .BAK");
  ui_line(12, "Do not power off");
  ui_line(13, "during SD activity.");
  ui_line(15, "A / B: Return");
  while (!(ui_key() & (J_A | J_B))) {
  }
}

void save_progress(uint8_t stage) {
  ui_page("Saving to SD");
  switch (stage) {
  case ST_CHECK_SOURCE:
    ui_line(3, "Checking original");
    break;
  case ST_BACKUP:
    ui_line(3, "Creating backup");
    break;
  case ST_VERIFY_BACKUP:
    ui_line(3, "Verifying backup");
    break;
  case ST_WRITE:
    ui_line(3, "Writing edited save");
    break;
  case ST_VERIFY_SAVE:
    ui_line(3, "Verifying edited save");
    break;
  case ST_FINISHED:
    ui_line(3, "SAVE VERIFIED");
    break;
  }
  ui_line(7, "DO NOT POWER OFF");
  ui_line(8, "DO NOT REMOVE CARD");
}
