#ifndef YELLOW_H
#define YELLOW_H

#include <stdint.h>
#ifdef __SDCC
#include <gb/gb.h>
#define YELLOW_BANKED BANKED
#else
#define YELLOW_BANKED
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define YELLOW_SPECIES_COUNT 151u
#define YELLOW_MOVE_COUNT 165u
#define YELLOW_NUM_BOXES 12u
#define YELLOW_PARTY_CAPACITY 6u
#define YELLOW_BOX_CAPACITY 20u
#define YELLOW_BAG_CAPACITY 20u
#define YELLOW_MAX_QTY 99u
#define YELLOW_MAX_MONEY 999999ul
#define YELLOW_MIN_LEVEL 1u
#define YELLOW_MAX_LEVEL 100u
#define YELLOW_MAX_DV 15u
#define YELLOW_MAX_STAT_EXP 65535u
#define YELLOW_MAX_PP_UPS 3u
#define YELLOW_MOVE_SLOTS 4u
#define YELLOW_STAT_COUNT 5u
#define YELLOW_NAME_FIELD 11u
#define YELLOW_NAME_CHARS 10u
#define YELLOW_BADGE_COUNT 8u
#define YELLOW_MAX_EXP 0xFFFFFFul
#define YELLOW_PIKACHU_INDEX 0x54u

#define YELLOW_LOC_PARTY 0u
#define YELLOW_LOC_BOX(n) ((uint8_t)((n) + 1u))

typedef enum {
  YELLOW_STAT_HP = 0,
  YELLOW_STAT_ATTACK = 1,
  YELLOW_STAT_DEFENSE = 2,
  YELLOW_STAT_SPEED = 3,
  YELLOW_STAT_SPECIAL = 4
} YellowStat;

typedef enum {
  YELLOW_BADGE_BOULDER = 0,
  YELLOW_BADGE_CASCADE = 1,
  YELLOW_BADGE_THUNDER = 2,
  YELLOW_BADGE_RAINBOW = 3,
  YELLOW_BADGE_SOUL = 4,
  YELLOW_BADGE_MARSH = 5,
  YELLOW_BADGE_VOLCANO = 6,
  YELLOW_BADGE_EARTH = 7
} YellowBadge;

typedef enum {
  YE_OK = 0,
  YE_ERR_NOT_INITIALIZED = 1,
  YE_ERR_MALFORMED_SAVE = 2,
  YE_ERR_BAD_LOCATION = 3,
  YE_ERR_BAD_SLOT = 4,
  YE_ERR_EMPTY_SLOT = 5,
  YE_ERR_BAD_SPECIES = 6,
  YE_ERR_BAD_LEVEL = 7,
  YE_ERR_BAD_MOVE = 8,
  YE_ERR_BAD_MOVE_INDEX = 9,
  YE_ERR_BAD_STAT = 10,
  YE_ERR_BAD_DV = 11,
  YE_ERR_BAD_BAG_INDEX = 12,
  YE_ERR_BAG_FULL = 13,
  YE_ERR_BAD_ITEM = 14,
  YE_ERR_MONEY_RANGE = 15,
  YE_ERR_TEXT_UNENCODABLE = 16,
  YE_ERR_TEXT_TOO_LONG = 17,
  YE_ERR_BOXES_UNINITIALIZED = 18,
  YE_ERR_BAD_PP_UPS = 19,
  YE_ERR_BAD_BADGE = 20,
  YE_ERR_MOVE_WIPE_ALL = 21
} YellowError;

#define YE_F_MAIN_CHECKSUM 0x0001u
#define YE_F_BANK2_CHECKSUM 0x0002u
#define YE_F_BANK3_CHECKSUM 0x0004u
#define YE_F_BOX_CHECKSUM 0x0008u
#define YE_F_BOXES_UNINITIALIZED 0x0010u

typedef struct YellowPokemonView {
  uint8_t valid;
  uint8_t species;
  uint8_t dex;
  uint8_t level;
  uint32_t exp;
  uint8_t types[2];
  uint8_t catch_rate;
  uint16_t current_hp;
  uint8_t status;
  uint8_t moves[4];
  uint8_t move_count;
  uint8_t pp[4];
  uint8_t dvs[2];
  uint16_t stat_exp[5];
  uint16_t max_hp;
  uint16_t attack;
  uint16_t defense;
  uint16_t speed;
  uint16_t special;
} YellowPokemonView;

typedef struct YellowBaseStats {
  uint8_t hp;
  uint8_t attack;
  uint8_t defense;
  uint8_t speed;
  uint8_t special;
  uint8_t type1;
  uint8_t type2;
  uint8_t catch_rate;
  uint8_t exp_yield;
  uint8_t growth_rate;
} YellowBaseStats;

YellowError yellow_init(void) YELLOW_BANKED;

uint16_t yellow_status_flags(void) YELLOW_BANKED;

uint8_t yellow_box_checksum_ok(uint8_t n) YELLOW_BANKED;

uint8_t yellow_is_yellow_hint(void) YELLOW_BANKED;

uint8_t yellow_main_dirty(void) YELLOW_BANKED;
uint16_t yellow_dirty_box_mask(void) YELLOW_BANKED;
uint8_t yellow_is_dirty(void) YELLOW_BANKED;

void yellow_flush(void) YELLOW_BANKED;
void yellow_mark_saved(void) YELLOW_BANKED;

void yellow_error_string(YellowError err, char *buf,
                         uint8_t buflen) YELLOW_BANKED;

uint8_t yellow_party_count(void) YELLOW_BANKED;

uint8_t yellow_box_count(uint8_t n) YELLOW_BANKED;

uint8_t yellow_current_box(void) YELLOW_BANKED;

uint8_t yellow_boxes_initialized(void) YELLOW_BANKED;

void yellow_get_pokemon(uint8_t loc, uint8_t slot,
                        YellowPokemonView *out) YELLOW_BANKED;

void yellow_get_nickname(uint8_t loc, uint8_t slot, uint8_t *out) YELLOW_BANKED;

YellowError yellow_set_species(uint8_t loc, uint8_t slot,
                               uint8_t species_index) YELLOW_BANKED;

YellowError yellow_set_level(uint8_t loc, uint8_t slot,
                             uint8_t level) YELLOW_BANKED;

YellowError yellow_set_exp(uint8_t loc, uint8_t slot,
                           uint32_t exp) YELLOW_BANKED;

YellowError yellow_set_move(uint8_t loc, uint8_t slot, uint8_t move_index,
                            uint8_t move_id) YELLOW_BANKED;

YellowError yellow_set_move_pp(uint8_t loc, uint8_t slot, uint8_t move_index,
                               uint8_t pp) YELLOW_BANKED;

YellowError yellow_set_pp_ups(uint8_t loc, uint8_t slot, uint8_t move_index,
                              uint8_t ups) YELLOW_BANKED;

YellowError yellow_set_dvs(uint8_t loc, uint8_t slot, uint8_t attack,
                           uint8_t defense, uint8_t speed,
                           uint8_t special) YELLOW_BANKED;

YellowError yellow_set_stat_exp(uint8_t loc, uint8_t slot, YellowStat stat,
                                uint16_t value) YELLOW_BANKED;

YellowError yellow_recalculate_stats(uint8_t loc, uint8_t slot) YELLOW_BANKED;

YellowError yellow_set_nickname(uint8_t loc, uint8_t slot,
                                const uint8_t *raw) YELLOW_BANKED;

uint32_t yellow_get_money(void) YELLOW_BANKED;

YellowError yellow_set_money(uint32_t value) YELLOW_BANKED;

uint8_t yellow_get_badges(void) YELLOW_BANKED;
void yellow_set_badges(uint8_t badges) YELLOW_BANKED;
uint8_t yellow_has_badge(YellowBadge badge) YELLOW_BANKED;
void yellow_set_badge(YellowBadge badge, uint8_t obtained) YELLOW_BANKED;

uint8_t yellow_pikachu_friendship(void) YELLOW_BANKED;
void yellow_set_pikachu_friendship(uint8_t value) YELLOW_BANKED;

uint8_t yellow_bag_count(void) YELLOW_BANKED;

void yellow_bag_get(uint8_t index, uint8_t *item_id,
                    uint8_t *qty) YELLOW_BANKED;

YellowError yellow_bag_set_item(uint8_t index, uint8_t item_id) YELLOW_BANKED;

YellowError yellow_bag_set_qty(uint8_t index, uint8_t qty) YELLOW_BANKED;

YellowError yellow_bag_add(uint8_t item_id, uint8_t qty) YELLOW_BANKED;

YellowError yellow_bag_remove(uint8_t index) YELLOW_BANKED;

char yellow_decode_byte(uint8_t raw) YELLOW_BANKED;

YellowError yellow_encode_text(const char *in, uint8_t *out,
                               uint8_t *bad_index) YELLOW_BANKED;

void yellow_species_name(uint8_t dex, char *buf, uint8_t buflen) YELLOW_BANKED;
void yellow_move_name(uint8_t move_id, char *buf, uint8_t buflen) YELLOW_BANKED;
void yellow_item_name(uint8_t item_id, char *buf, uint8_t buflen) YELLOW_BANKED;
void yellow_type_name(uint8_t type_id, char *buf, uint8_t buflen) YELLOW_BANKED;

uint8_t yellow_item_valid(uint8_t item_id) YELLOW_BANKED;
uint8_t yellow_move_valid(uint8_t move_id) YELLOW_BANKED;

uint8_t yellow_move_power(uint8_t move_id) YELLOW_BANKED;
uint8_t yellow_move_type(uint8_t move_id) YELLOW_BANKED;
uint8_t yellow_move_accuracy(uint8_t move_id) YELLOW_BANKED;
uint8_t yellow_move_base_pp(uint8_t move_id) YELLOW_BANKED;

void yellow_base_stats(uint8_t dex, YellowBaseStats *out) YELLOW_BANKED;

uint8_t yellow_dex_to_index(uint8_t dex) YELLOW_BANKED;
uint8_t yellow_index_to_dex(uint8_t species_index) YELLOW_BANKED;

uint32_t yellow_exp_for_level(uint8_t growth_rate, uint8_t level) YELLOW_BANKED;
uint8_t yellow_level_for_exp(uint8_t growth_rate, uint32_t exp) YELLOW_BANKED;

uint16_t yellow_calc_stat(uint8_t base, uint8_t dv, uint16_t stat_exp,
                          uint8_t level, uint8_t is_hp) YELLOW_BANKED;

uint8_t yellow_max_pp(uint8_t base_pp, uint8_t ups) YELLOW_BANKED;

#ifdef __cplusplus
}
#endif

#endif
