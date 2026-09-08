#include "browser.h"
#include "ui.h"
#include "x7_io.h"
#include <gb/gb.h>
#include <gbdk/console.h>
#include <stdio.h>
#ifdef __SDCC
#pragma bank 7
#endif

#define BAR_WIDTH 18
#define BAR_ROW 5
#define PERCENT_ROW 6
#define COUNT_ROW 7

static uint8_t last_stage = 255, tick;
static uint16_t last_percent;

static void render_chars(uint8_t x, uint8_t y, const char *text) {
  gotoxy(x, y);
  while (*text)
    putchar(*text++);
}

static void render_number(uint8_t x, uint8_t y, uint16_t value) {
  char digits[6];
  uint8_t i = 5;
  digits[i] = 0;
  do {
    digits[--i] = '0' + value % 10;
    value /= 10;
  } while (value);
  gotoxy(x, y);
  while (i < 5)
    putchar(digits[i++]);
}

static const char *stage_title(uint8_t stage) {
  switch (stage) {
  case ST_MOUNT:
  case ST_BROWSE:
    return "SD card";
  case ST_LOAD:
    return "Loading save";
  case ST_VALIDATE:
    return "Checking save";
  default:
    return "Saving to SD";
  }
}

static const char *stage_label(uint8_t stage) {
  switch (stage) {
  case ST_MOUNT:
    return "Mounting card";
  case ST_BROWSE:
    return "Browsing card";
  case ST_LOAD:
    return "Reading save file";
  case ST_BACKUP:
    return "Creating backup";
  case ST_VERIFY_BACKUP:
    return "Verifying backup";
  case ST_CHECK_SOURCE:
    return "Checking original";
  case ST_FIND_BACKUP:
    return "Finding backup slot";
  case ST_SYNC_BACKUP:
    return "Syncing backup";
  case ST_WRITE:
    return "Writing edited save";
  case ST_SYNC_SAVE:
    return "Syncing save";
  case ST_VERIFY_SAVE:
    return "Verifying save";
  case ST_VALIDATE:
    return "Validating save data";
  case ST_PREPARE:
    return "Preparing card write";
  case ST_FINISHED:
    return "SAVE VERIFIED";
  default:
    return "Working";
  }
}

static uint8_t stage_dangerous(uint8_t stage) {
  switch (stage) {
  case ST_CHECK_SOURCE:
  case ST_BACKUP:
  case ST_VERIFY_BACKUP:
  case ST_FIND_BACKUP:
  case ST_SYNC_BACKUP:
  case ST_WRITE:
  case ST_SYNC_SAVE:
  case ST_VERIFY_SAVE:
  case ST_PREPARE:
  case ST_FINISHED:
    return 1;
  default:
    return 0;
  }
}

static void progress_stage(uint8_t stage) {
  ui_page(stage_title(stage));
  render_chars(0, 3, stage_label(stage));
  render_chars(0, BAR_ROW, "[");
  render_chars(1 + BAR_WIDTH, BAR_ROW, "]");
  if (stage_dangerous(stage)) {
    render_chars(0, 9, "DO NOT POWER OFF");
    render_chars(0, 10, "DO NOT REMOVE CARD");
  }
  last_stage = stage;
  last_percent = 9999;
}

static void progress_known(uint16_t completed, uint16_t total) {
  uint8_t filled, x;
  uint16_t percent = (uint16_t)((uint32_t)completed * 100u / total);
  if (percent != last_percent) {
    filled = (uint8_t)(percent * BAR_WIDTH / 100u);
    render_number(0, PERCENT_ROW, percent);
    putchar('%');
    gotoxy(1, BAR_ROW);
    for (x = 0; x < BAR_WIDTH; ++x)
      putchar(x < filled ? '#' : ' ');
    last_percent = percent;
  }
}

static void progress_unknown(uint16_t completed) {
  uint8_t x, position = tick++ % (BAR_WIDTH - 2);
  gotoxy(1, BAR_ROW);
  for (x = 0; x < BAR_WIDTH; ++x)
    putchar(x >= position && x < position + 3 ? '#' : ' ');
  render_chars(0, COUNT_ROW, "Processed:");
  render_number(11, COUNT_ROW, completed);
}

static void storage_progress_render(uint8_t stage, uint16_t completed,
                                    uint16_t total) BANKED {
  if (stage != last_stage || !completed)
    progress_stage(stage);
  if (stage == ST_FINISHED) {
    ui_line(BAR_ROW, "SAVE VERIFIED");
    return;
  }
  if (!total) {
    progress_unknown(completed);
    return;
  }
  if (completed == 1 && total == 1) {
    ui_line(BAR_ROW, "Done");
    ui_line(PERCENT_ROW, "100%");
    return;
  }
  progress_known(completed, total);
}

void storage_progress(uint8_t stage, uint16_t completed,
                      uint16_t total) NONBANKED {
  storage_progress_render(stage, completed, total);
}

void show_storage_error(uint8_t result) BANKED {
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
  last_stage = 255;
  last_percent = 9999;
  while (!(ui_key() & (J_A | J_B))) {
  }
}
