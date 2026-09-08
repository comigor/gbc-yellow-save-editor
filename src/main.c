#include "browser.h"
#include "graphics.h"
#include "storage.h"
#include "text.h"
#include "ui.h"
#include "yellow.h"
#include <gb/gb.h>
#include <gbdk/console.h>
#include <stdio.h>
#include <string.h>
#ifdef __SDCC
#pragma bank 5
#endif

static char path[STORAGE_PATH_MAX], text[21], nickname[11];
static uint8_t raw_name[11];
static YellowPokemonView pokemon;
static uint32_t number;
static const char *const stats[] = {"HP", "Attack", "Defense", "Speed",
                                    "Special"};
static const char *const badges[] = {"Boulder", "Cascade", "Thunder", "Rainbow",
                                     "Soul",    "Marsh",   "Volcano", "Earth"};

static void core_error(YellowError error) {
  if (!error)
    return;
  yellow_error_string(error, text, sizeof(text));
  ui_notice("Cannot apply edit", text);
}

static void species_label(uint16_t value, char *out) {
  yellow_species_name((uint8_t)value, out, 20);
}
static void move_label(uint16_t value, char *out) {
  if (!value)
    strcpy(out, "(empty)");
  else
    yellow_move_name((uint8_t)value, out, 20);
}
static void item_label(uint16_t value, char *out) {
  yellow_item_name((uint8_t)value, out, 20);
  if (!out[0])
    strcpy(out, "(not an item)");
}

static void get_nickname(uint8_t location, uint8_t slot) {
  uint8_t i;
  yellow_get_nickname(location, slot, raw_name);
  for (i = 0; i < 10; ++i) {
    nickname[i] = yellow_decode_byte(raw_name[i]);
    if (!nickname[i])
      break;
  }
  nickname[10] = 0;
}

static uint8_t edit_moves(uint8_t location, uint8_t slot) {
  uint8_t selection = 0, key, move, kind, base;
  for (;;) {
    yellow_get_pokemon(location, slot, &pokemon);
    ui_page("Moves / PP");
    for (move = 0; move < 4; ++move) {
      move_label(pokemon.moves[move], text);
      gotoxy(0, 3 + move * 2);
      printf("%c%u %s", (char)(selection == move * 3 ? '>' : ' '),
             (unsigned int)(move + 1), text);
      gotoxy(0, 4 + move * 2);
      printf("%cPP %u  %cUps %u", (char)(selection == move * 3 + 1 ? '>' : ' '),
             (unsigned int)(pokemon.pp[move] & 63),
             (char)(selection == move * 3 + 2 ? '>' : ' '),
             (unsigned int)(pokemon.pp[move] >> 6));
    }
    ui_line(14, "Up/Down: field");
    ui_line(15, "A: Edit  B: Back");
    ui_line(16, "Left/Right: Tabs");
    key = ui_key();
    if (key & (J_B | J_LEFT | J_RIGHT))
      return key;
    if (key & J_UP) {
      if (selection)
        --selection;
    } else if (key & J_DOWN) {
      if (selection < 11)
        ++selection;
    } else if (key & J_A) {
      move = selection / 3;
      kind = selection % 3;
      if (!kind) {
        number = pokemon.moves[move];
        if (ui_edit_number("Move", &number, 0, YELLOW_MOVE_COUNT, move_label))
          core_error(yellow_set_move(location, slot, move, (uint8_t)number));
      } else if (kind == 1) {
        number = pokemon.pp[move] & 63;
        base = yellow_move_base_pp(pokemon.moves[move]);
        if (ui_edit_number("Current PP", &number, 0,
                           yellow_max_pp(base, pokemon.pp[move] >> 6), 0))
          core_error(yellow_set_move_pp(location, slot, move, (uint8_t)number));
      } else {
        number = pokemon.pp[move] >> 6;
        if (ui_edit_number("PP Ups", &number, 0, 3, 0))
          core_error(yellow_set_pp_ups(location, slot, move, (uint8_t)number));
      }
    }
  }
}

static uint8_t edit_training(uint8_t location, uint8_t slot, uint8_t dv_mode) {
  uint8_t selection = 0, key, i, dvs[4];
  for (;;) {
    yellow_get_pokemon(location, slot, &pokemon);
    dvs[0] = pokemon.dvs[0] >> 4;
    dvs[1] = pokemon.dvs[0] & 15;
    dvs[2] = pokemon.dvs[1] >> 4;
    dvs[3] = pokemon.dvs[1] & 15;
    ui_page(dv_mode ? "DVs" : "Stat experience");
    for (i = 0; i < (dv_mode ? 4 : 5); ++i) {
      gotoxy(0, 3 + i * 2);
      printf("%c%s ", (char)(selection == i ? '>' : ' '),
             stats[i + (dv_mode ? 1 : 0)]);
      ui_number(dv_mode ? dvs[i] : pokemon.stat_exp[i]);
    }
    if (dv_mode) {
      gotoxy(0, 12);
      printf("HP DV %u (derived)",
             (unsigned int)(((dvs[0] & 1) << 3) | ((dvs[1] & 1) << 2) |
                            ((dvs[2] & 1) << 1) | (dvs[3] & 1)));
    }
    ui_line(15, "A: Edit  B: Back");
    ui_line(16, "Left/Right: Tabs");
    key = ui_key();
    if (key & (J_B | J_LEFT | J_RIGHT))
      return key;
    if (key & J_UP) {
      if (selection)
        --selection;
    } else if (key & J_DOWN) {
      if (selection + 1 < (dv_mode ? 4 : 5))
        ++selection;
    } else if (key & J_A) {
      number = dv_mode ? dvs[selection] : pokemon.stat_exp[selection];
      if (ui_edit_number(dv_mode ? "DV" : "Stat experience", &number, 0,
                         dv_mode ? 15 : 65535UL, 0)) {
        if (dv_mode) {
          dvs[selection] = (uint8_t)number;
          core_error(
              yellow_set_dvs(location, slot, dvs[0], dvs[1], dvs[2], dvs[3]));
        } else
          core_error(yellow_set_stat_exp(location, slot, (YellowStat)selection,
                                         (uint16_t)number));
      }
    }
  }
}

static void edit_pokemon(uint8_t location, uint8_t slot) {
  uint8_t selection = 0, key, tab = 0;
  for (;;) {
    if (tab) {
      key = tab == 1 ? edit_moves(location, slot)
                     : edit_training(location, slot, tab == 2);
      if (key & J_B)
        return;
      tab = (key & J_RIGHT) ? (tab + 1) % 4 : tab - 1;
      continue;
    }
    yellow_get_pokemon(location, slot, &pokemon);
    if (!pokemon.valid) {
      ui_notice("Invalid Pokemon", "This slot cannot\nbe edited.");
      return;
    }
    get_nickname(location, slot);
    ui_page(nickname);
    species_label(pokemon.dex, text);
    ui_line(1, "Summary    1/4");
    ui_line(2, text);
    gotoxy(0, 4);
    printf("Level %u", (unsigned int)pokemon.level);
    gotoxy(0, 5);
    printf("EXP ");
    ui_number(pokemon.exp);
    gotoxy(0, 7);
    printf("HP %u/%u", (unsigned int)pokemon.current_hp,
           (unsigned int)pokemon.max_hp);
    gotoxy(0, 8);
    printf("ATK %u", (unsigned int)pokemon.attack);
    gotoxy(0, 9);
    printf("DEF %u", (unsigned int)pokemon.defense);
    gotoxy(0, 10);
    printf("SPD %u", (unsigned int)pokemon.speed);
    gotoxy(0, 11);
    printf("SPC %u", (unsigned int)pokemon.special);
#ifdef YELLOW_GRAPHICS
    graphics_front(pokemon.dex);
#endif
    gotoxy(0, 13);
    printf("%cSpecies %cNickname", (char)(selection == 0 ? '>' : ' '),
           (char)(selection == 1 ? '>' : ' '));
    gotoxy(0, 14);
    printf("%cLevel   %cEXP", (char)(selection == 2 ? '>' : ' '),
           (char)(selection == 3 ? '>' : ' '));
    ui_line(15, "A: Edit  B: Back");
    ui_line(16, "Left/Right: Tabs");
    key = ui_key();
    if (key & J_B)
      return;
    if (key & (J_LEFT | J_RIGHT)) {
      tab = (key & J_RIGHT) ? 1 : 3;
      continue;
    }
    if (key & J_UP) {
      if (selection)
        --selection;
    } else if (key & J_DOWN) {
      if (selection < 3)
        ++selection;
    } else if (key & J_A) {
      switch (selection) {
      case 0:
        number = pokemon.dex;
        if (ui_edit_number("Species (Dex number)", &number, 1, 151,
                           species_label))
          core_error(yellow_set_species(location, slot,
                                        yellow_dex_to_index((uint8_t)number)));
        break;
      case 1:
        if (ui_edit_name(nickname)) {
          YellowError error = yellow_encode_text(nickname, raw_name, 0);
          if (!error)
            error = yellow_set_nickname(location, slot, raw_name);
          core_error(error);
        }
        break;
      case 2:
        number = pokemon.level;
        if (ui_edit_number("Level (updates EXP)", &number, 1, 100, 0))
          core_error(yellow_set_level(location, slot, (uint8_t)number));
        break;
      case 3:
        number = pokemon.exp;
        if (ui_edit_number("EXP (updates level)", &number, 0, YELLOW_MAX_EXP,
                           0))
          core_error(yellow_set_exp(location, slot, number));
        break;
      }
    }
  }
}

static void pokemon_list(void) {
  uint8_t location = 0, selection = 0, count, key, i, first;
  for (;;) {
    count = location ? yellow_box_count(location - 1) : yellow_party_count();
    if (selection >= count)
      selection = 0;
    ui_page("Pokemon storage");
    gotoxy(0, 2);
    if (location)
      printf("Box %u%s", (unsigned int)location,
             location - 1 == yellow_current_box() ? " (current)" : "");
    else
      printf("Party");
#ifdef YELLOW_GRAPHICS
    first = (selection / 5) * 5;
    for (i = first; i < count && i < first + 5; ++i) {
      yellow_get_pokemon(location, i, &pokemon);
      graphics_icon(pokemon.dex, i - first);
      get_nickname(location, i);
      gotoxy(0, 3 + (i - first) * 2);
      printf("%c", (char)(selection == i ? '>' : ' '));
      gotoxy(4, 3 + (i - first) * 2);
      printf("%u %s", (unsigned int)(i + 1), nickname);
    }
#else
    first = selection & 0xf8u;
    for (i = first; i < count && i < first + 8; ++i) {
      get_nickname(location, i);
      gotoxy(0, 4 + i - first);
      printf("%c%u %s", (char)(selection == i ? '>' : ' '),
             (unsigned int)(i + 1), nickname);
    }
#endif
    if (!count)
      ui_line(5, "Empty");
    ui_line(13, "Left/Right: box");
    ui_line(14, "Up/Down: Pokemon");
    ui_line(15, "A: Edit  B: Back");
    key = ui_key();
    if (key & J_B)
      return;
    if (key & J_LEFT) {
      location = location ? location - 1 : 12;
      selection = 0;
    } else if (key & J_RIGHT) {
      location = location == 12 ? 0 : location + 1;
      selection = 0;
    } else if (key & J_UP) {
      if (selection)
        --selection;
    } else if (key & J_DOWN) {
      if (selection + 1 < count)
        ++selection;
    } else if ((key & J_A) && count)
      edit_pokemon(location, selection);
  }
}

static void edit_badges(void) {
  uint8_t selection = 0, key, i, mask;
  for (;;) {
    mask = yellow_get_badges();
    ui_page("Badges");
    for (i = 0; i < 8; ++i) {
      gotoxy(0, 3 + i);
      printf("%c[%c] %s", (char)(i == selection ? '>' : ' '),
             (char)(mask & (1u << i) ? 'X' : ' '), badges[i]);
    }
    ui_line(15, "A: Toggle  B: Back");
    key = ui_key();
    if (key & J_B)
      return;
    if (key & J_UP) {
      if (selection)
        --selection;
    } else if (key & J_DOWN) {
      if (selection < 7)
        ++selection;
    } else if (key & J_A)
      yellow_set_badges(mask ^ (1u << selection));
  }
}

static void trainer(void) {
  uint8_t selection = 0, key;
  for (;;) {
    ui_page("Trainer");
    gotoxy(0, 3);
    printf("%cMoney ", (char)(selection == 0 ? '>' : ' '));
    ui_number(yellow_get_money());
    gotoxy(0, 5);
    printf("%cBadges", (char)(selection == 1 ? '>' : ' '));
    gotoxy(0, 7);
    printf("%cPikachu friendship", (char)(selection == 2 ? '>' : ' '));
    gotoxy(2, 8);
    printf("%u", (unsigned int)yellow_pikachu_friendship());
    ui_line(15, "A: Edit  B: Back");
    key = ui_key();
    if (key & J_B)
      return;
    if (key & J_UP) {
      if (selection)
        --selection;
    } else if (key & J_DOWN) {
      if (selection < 2)
        ++selection;
    } else if (key & J_A) {
      if (selection == 0) {
        number = yellow_get_money();
        if (ui_edit_number("Money", &number, 0, YELLOW_MAX_MONEY, 0))
          core_error(yellow_set_money(number));
      } else if (selection == 1)
        edit_badges();
      else {
        number = yellow_pikachu_friendship();
        if (ui_edit_number("Pikachu friendship", &number, 0, 255, 0))
          yellow_set_pikachu_friendship((uint8_t)number);
      }
    }
  }
}

static void edit_bag_item(uint8_t slot, uint8_t new_item) {
  uint8_t item = 1, qty = 1, key, selection = 0;
  if (!new_item)
    yellow_bag_get(slot, &item, &qty);
  for (;;) {
    ui_page(new_item ? "Add bag item" : "Bag item");
    item_label(item, text);
    gotoxy(0, 3);
    printf("%c%s", (char)(selection == 0 ? '>' : ' '), text);
    gotoxy(0, 5);
    printf("%cQuantity %u", (char)(selection == 1 ? '>' : ' '),
           (unsigned int)qty);
    gotoxy(0, 7);
    printf("%c%s", (char)(selection == 2 ? '>' : ' '),
           new_item ? "Add item" : "Remove item");
    ui_line(15, "A: Select  B: Back");
    key = ui_key();
    if (key & J_B)
      return;
    if (key & J_UP) {
      if (selection)
        --selection;
    } else if (key & J_DOWN) {
      if (selection < 2)
        ++selection;
    } else if (key & J_A) {
      if (selection == 0) {
        number = item;
        if (ui_edit_number("Bag item ID", &number, 1, 250, item_label)) {
          if (!yellow_item_valid((uint8_t)number)) {
            ui_notice("Invalid item", "Choose a named item.");
            continue;
          }
          item = (uint8_t)number;
          if (!new_item)
            core_error(yellow_bag_set_item(slot, item));
        }
      } else if (selection == 1) {
        number = qty;
        if (ui_edit_number("Quantity", &number, 1, 99, 0)) {
          qty = (uint8_t)number;
          if (!new_item)
            core_error(yellow_bag_set_qty(slot, qty));
        }
      } else if (new_item) {
        core_error(yellow_bag_add(item, qty));
        return;
      } else if (ui_confirm("Remove bag item?", text)) {
        core_error(yellow_bag_remove(slot));
        return;
      }
    }
  }
}

static void bag(void) {
  uint8_t selection = 0, key, count, first, i, item, qty;
  for (;;) {
    count = yellow_bag_count();
    if (selection > count)
      selection = count;
    ui_page("Bag");
    first = selection & 0xf8u;
    for (i = first; i <= count && i < first + 8; ++i) {
      gotoxy(0, 3 + i - first);
      putchar(selection == i ? '>' : ' ');
      if (i == count)
        printf("+ Add item");
      else {
        yellow_bag_get(i, &item, &qty);
        item_label(item, text);
        printf("%s x%u", text, (unsigned int)qty);
      }
    }
    ui_line(14, "Up/Down: item");
    ui_line(15, "A: Edit  B: Back");
    key = ui_key();
    if (key & J_B)
      return;
    if (key & J_UP) {
      if (selection)
        --selection;
    } else if (key & J_DOWN) {
      if (selection < count)
        ++selection;
    } else if (key & J_A)
      edit_bag_item(selection, selection == count);
  }
}

static uint8_t save_changes(void) {
  uint8_t result;
  if (!yellow_is_dirty()) {
    ui_notice("Nothing changed", "SD file untouched.");
    return 1;
  }
  if (!ui_confirm("Write edited save?",
                  "Creates a .BAK first\nthen overwrites save.\n\nFAT32 is NOT "
                  "atomic.\nPower loss can\ndamage the card."))
    return 1;
  yellow_flush();
  result = storage_commit(save_progress);
  if (result) {
    show_storage_error(result);
    return 0;
  }
  yellow_mark_saved();
  ui_page("SAVE VERIFIED");
  ui_line(3, "Original backup:");
  ui_line(4, text_last(storage_backup, '/') ? text_last(storage_backup, '/') + 1
                                            : storage_backup);
  ui_line(7, "Reload Yellow from");
  ui_line(8, "the EverDrive menu.");
  ui_line(9, "Do not load an old");
  ui_line(10, "save state over it.");
  ui_line(15, "A / B: Return");
  while (!(ui_key() & (J_A | J_B))) {
  }
  return 1;
}

void editor_run(void) BANKED {
  uint8_t result, key, selection;
  uint16_t flags;
  YellowError error;
  ui_page("YELLOW SAVE EDITOR");
  ui_line(3, "Chromatic / X7");
  ui_line(4, "English Yellow");
  ui_line(6, "Flush Yellow's save");
  ui_line(7, "via X7 menu first.");
  ui_line(9, "Keep an independent");
  ui_line(10, "backup of your card.");
  ui_line(13, "START: Browse SD");
  while (!(ui_key() & J_START)) {
  }
  for (;;) {
    result = storage_mount();
    if (result) {
      show_storage_error(result);
      return;
    }
    if (!browser_choose(path))
      return;
    ui_page("Loading save...");
    result = storage_load(path);
    if (result) {
      show_storage_error(result);
      return;
    }
    error = yellow_init();
    if (error) {
      core_error(error);
      continue;
    }
    flags = yellow_status_flags();
    if (flags & (YE_F_MAIN_CHECKSUM | YE_F_BANK2_CHECKSUM |
                 YE_F_BANK3_CHECKSUM | YE_F_BOX_CHECKSUM)) {
      ui_notice("Save rejected",
                "Invalid structure or\nchecksum. Original\nwas not modified.");
      continue;
    }
    if (!yellow_is_yellow_hint() &&
        !ui_confirm("Confirm game",
                    "Cannot identify game\nfrom save alone.\n\nIs this "
                    "English\nPokemon Yellow?"))
      continue;
    selection = 0;
    for (;;) {
      ui_page("Yellow editor");
      gotoxy(0, 3);
      printf("%cPokemon / boxes", (char)(selection == 0 ? '>' : ' '));
      gotoxy(0, 5);
      printf("%cTrainer", (char)(selection == 1 ? '>' : ' '));
      gotoxy(0, 7);
      printf("%cBag", (char)(selection == 2 ? '>' : ' '));
      gotoxy(0, 9);
      printf("%cSave to SD", (char)(selection == 3 ? '>' : ' '));
      ui_line(12, yellow_is_dirty() ? "UNSAVED CHANGES" : "No pending changes");
      ui_line(14, "A: Open START: Save");
      ui_line(15, "B: Choose other save");
      key = ui_key();
      if (key & J_B) {
        if (!yellow_is_dirty() ||
            ui_confirm("Discard changes?", "SD file will remain\nunchanged."))
          break;
      } else if (key & J_UP) {
        if (selection)
          --selection;
      } else if (key & J_DOWN) {
        if (selection < 3)
          ++selection;
      } else if (key & J_START) {
        if (!save_changes())
          return;
      } else if (key & J_A) {
        if (selection == 0)
          pokemon_list();
        else if (selection == 1)
          trainer();
        else if (selection == 2)
          bag();
        else if (!save_changes())
          return;
      }
    }
  }
}
