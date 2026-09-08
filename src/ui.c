#include "ui.h"
#include <gb/gb.h>
#include <gbdk/console.h>
#include <stdio.h>
#include <string.h>

static char number_text[11], choice_text[20];
static uint8_t previous_key, repeat_frames;

uint8_t ui_key(void) {
  uint8_t key;
  for (;;) {
    vsync();
    key = joypad();
    if (!key) {
      previous_key = repeat_frames = 0;
      continue;
    }
    if (key != previous_key) {
      previous_key = key;
      repeat_frames = 0;
      return key;
    }
    if (key & (J_UP | J_DOWN | J_LEFT | J_RIGHT)) {
      if (++repeat_frames >= 18) {
        repeat_frames = 14;
        return key;
      }
    }
  }
}

void ui_page(const char *title) {
  cls();
  printf("%s", title);
  ui_line(1, "--------------------");
}

void ui_line(uint8_t row, const char *text) {
  uint8_t i;
  gotoxy(0, row);
  for (i = 0; i < 20; ++i) {
    if (*text)
      putchar(*text++);
    else
      putchar(' ');
  }
}

void ui_number(uint32_t value) {
  uint8_t i = 10;
  number_text[i] = 0;
  do {
    number_text[--i] = '0' + value % 10UL;
    value /= 10UL;
  } while (value);
  printf("%s", number_text + i);
}

uint8_t ui_confirm(const char *title, const char *message) {
  uint8_t key;
  ui_page(title);
  gotoxy(0, 3);
  printf("%s", message);
  ui_line(14, "A: Confirm");
  ui_line(15, "B: Cancel");
  do {
    key = ui_key();
  } while (!(key & (J_A | J_B)));
  return (key & J_B) == 0;
}

void ui_notice(const char *title, const char *message) {
  ui_page(title);
  gotoxy(0, 3);
  printf("%s", message);
  ui_line(15, "A / B: Return");
  while (!(ui_key() & (J_A | J_B))) {
  }
}

uint8_t ui_edit_number(const char *title, uint32_t *value, uint32_t minimum,
                       uint32_t maximum, void (*label)(uint16_t, char *)) {
  uint32_t edited = *value, step = 1;
  uint8_t key;
  for (;;) {
    ui_page(title);
    gotoxy(0, 3);
    ui_number(edited);
    if (label) {
      label((uint16_t)edited, choice_text);
      ui_line(5, choice_text);
    }
    gotoxy(0, 8);
    printf("Step ");
    ui_number(step);
    ui_line(10, "Up/Down: value");
    ui_line(11, "Left/Right: step");
    ui_line(14, "A: Apply  B: Cancel");
    key = ui_key();
    if (key & J_B)
      return 0;
    if (key & J_A) {
      *value = edited;
      return 1;
    }
    if (key & J_RIGHT) {
      if (step <= maximum / 10UL)
        step *= 10UL;
    } else if (key & J_LEFT) {
      if (step > 1)
        step /= 10UL;
    } else if (key & J_UP)
      edited = maximum - edited < step ? maximum : edited + step;
    else if (key & J_DOWN)
      edited = edited - minimum < step ? minimum : edited - step;
  }
}

uint8_t ui_edit_name(char *name) {
  static const char alphabet[] =
      " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-?!.'";
  char edited[11];
  uint8_t cursor = 0, i, index, key;
  memset(edited, ' ', 10);
  edited[10] = 0;
  for (i = 0; i < 10 && name[i]; ++i)
    edited[i] = name[i];
  for (;;) {
    ui_page("Nickname");
    ui_line(4, edited);
    gotoxy(cursor, 5);
    putchar('^');
    ui_line(8, "Left/Right: position");
    ui_line(9, "Up/Down: character");
    ui_line(10, "SELECT: blank");
    ui_line(14, "A: Apply  B: Cancel");
    key = ui_key();
    if (key & J_B)
      return 0;
    if (key & J_A) {
      for (i = 10; i && edited[i - 1] == ' '; --i) {
      }
      edited[i] = 0;
      if (i) {
        strcpy(name, edited);
        return 1;
      }
      continue;
    }
    if (key & J_LEFT) {
      if (cursor)
        --cursor;
    } else if (key & J_RIGHT) {
      if (cursor < 9)
        ++cursor;
    } else if (key & J_SELECT)
      edited[cursor] = ' ';
    else if (key & (J_UP | J_DOWN)) {
      for (index = 0; alphabet[index] && alphabet[index] != edited[cursor];
           ++index) {
      }
      if (!alphabet[index])
        index = 0;
      if (key & J_UP)
        index = (index + 1) % (sizeof(alphabet) - 1);
      else
        index = index ? index - 1 : sizeof(alphabet) - 2;
      edited[cursor] = alphabet[index];
    }
  }
}
