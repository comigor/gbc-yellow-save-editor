#include "browser.h"
#include "text.h"
#include "ui.h"
#include <gb/gb.h>
#include <gbdk/console.h>
#include <stdio.h>
#include <string.h>
#ifdef __SDCC
#pragma bank 6
#endif

static char folder[STORAGE_PATH_MAX];
static char full_name[256];

static void show_name(uint8_t selection) {
  uint16_t position;
  storage_name(selection, full_name);
  ui_page("Full filename");
  gotoxy(0, 2);
  for (position = 0; full_name[position]; ++position)
    putchar(full_name[position]);
  ui_line(16, "A / B: Return");
  while (!(ui_key() & (J_A | J_B))) {
  }
}

uint8_t browser_choose(char *path) BANKED {
  uint16_t start = 0, length;
  uint8_t selection = 0, key, result, i, reload = 1;
  char *slash;
  strcpy(folder, "/");
  for (;;) {
    if (reload) {
      result = storage_list(folder, start);
      if (result) {
        show_storage_error(result);
        return 0;
      }
      reload = 0;
    }
    if (selection >= storage_count)
      selection = 0;
    ui_page("Choose Yellow save");
    length = strlen(folder);
    ui_line(2, folder + (length > 20 ? length - 20 : 0));
    for (i = 0; i < storage_count; ++i) {
      gotoxy(0, 4 + i);
      printf("%c%s%s", (char)(i == selection ? '>' : ' '),
             storage_entries[i].label, storage_entries[i].directory ? "/" : "");
    }
    if (!storage_count)
      ui_line(5, "No SAV/SRM files");
    ui_line(12, "SELECT: Full name");
    ui_line(13, "Left/Right: page");
    ui_line(14, "A: Open  B: Parent");
    ui_line(15, "START: Exit browser");
    key = ui_key();
    if (key & J_START)
      return 0;
    if (key & J_UP) {
      if (selection)
        --selection;
    } else if (key & J_DOWN) {
      if (selection + 1 < storage_count)
        ++selection;
    } else if (key & J_RIGHT) {
      if (storage_more && start <= 65527u) {
        start += STORAGE_PAGE;
        selection = 0;
        reload = 1;
      }
    } else if (key & J_LEFT) {
      if (start) {
        start -= STORAGE_PAGE;
        selection = 0;
        reload = 1;
      }
    } else if (key & J_B) {
      if (length > 1) {
        folder[length - 1] = 0;
        slash = text_last(folder, '/');
        slash[1] = 0;
        start = selection = 0;
        reload = 1;
      }
    } else if ((key & J_SELECT) && storage_count) {
      show_name(selection);
    } else if ((key & J_A) && storage_count) {
      if (length + strlen(storage_entries[selection].name) + 2 >=
          STORAGE_PATH_MAX) {
        ui_notice("Path too long", "Move save to a\nshallower directory.");
        continue;
      }
      strcpy(path, folder);
      strcat(path, storage_entries[selection].name);
      if (!storage_entries[selection].directory)
        return 1;
      strcpy(folder, path);
      strcat(folder, "/");
      start = selection = 0;
      reload = 1;
    }
  }
}
