#include "graphics.h"
#include "graphics_assets.h"

static const uint8_t *const front_banks[] = {
    &yellow_front_0[0][0], &yellow_front_1[0][0], &yellow_front_2[0][0],
    &yellow_front_3[0][0], &yellow_front_4[0][0], &yellow_front_5[0][0],
    &yellow_front_6[0][0], &yellow_front_7[0][0]};
static uint8_t tiles[49];

void graphics_init(void) NONBANKED {
  VBK_REG = 1;
  fill_bkg_rect(0, 0, 32, 32, 0);
  VBK_REG = 0;
}

void graphics_front(uint8_t dex) NONBANKED {
  uint8_t saved_bank = CURRENT_BANK;
  uint8_t group = (dex - 1) / 19;
  uint8_t slot = (dex - 1) % 19;
  uint8_t tile;
  SWITCH_ROM(8 + group);
  set_bkg_data(128, 49, front_banks[group] + (uint16_t)slot * 784);
  SWITCH_ROM(saved_bank);
  for (tile = 0; tile < 49; ++tile)
    tiles[tile] = 128 + tile;
  set_bkg_tiles(13, 5, 7, 7, tiles);
}

void graphics_icon(uint8_t dex, uint8_t row) NONBANKED {
  uint8_t saved_bank = CURRENT_BANK;
  uint8_t first = 177 + row * 4;
  uint8_t tile, icon;
  SWITCH_ROM(7);
  icon = yellow_menu_icon_ids[dex];
  set_bkg_data(first, 4, &yellow_menu_icons[icon][0]);
  SWITCH_ROM(saved_bank);
  for (tile = 0; tile < 4; ++tile)
    tiles[tile] = first + tile;
  set_bkg_tiles(1, 3 + row * 2, 2, 2, tiles);
}
