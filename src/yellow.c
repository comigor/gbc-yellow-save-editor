
#ifdef __SDCC
#pragma bank 3
#endif

#include "yellow.h"
#include "save_memory.h"

/* ---- offsets ---- */

#define OFF_BAG_COUNT 0x25C9u
#define OFF_BAG_ITEMS 0x25CAu /* 20 x [id, qty] + 0xFF */
#define OFF_MONEY 0x25F3u     /* 3 bytes big-endian BCD */
#define OFF_BADGES 0x2602u
#define OFF_CURRENT_BOX_NUM 0x284Cu /* bits 0-6 box, bit 7 init */
#define OFF_PIKACHU_FRIENDSHIP 0x271Cu
#define OFF_PLAYER_STARTER 0x29C3u
#define OFF_PARTY 0x2F2Cu       /* 0x194 bytes */
#define OFF_CURRENT_BOX 0x30C0u /* working copy, 0x462 bytes */
#define OFF_MAIN_CHK_START 0x2598u
#define OFF_MAIN_CHK_END 0x3522u
#define OFF_MAIN_CHECKSUM 0x3523u
#define OFF_BANK2_BOXES 0x4000u    /* boxes 0..5 */
#define OFF_BANK3_BOXES 0x6000u    /* boxes 6..11 */
#define OFF_BANK2_CHECKSUM 0x5A4Cu /* then 6 per-box bytes */
#define OFF_BANK3_CHECKSUM 0x7A4Cu

/* ---- record geometry ---- */

#define NAME_LEN 11u
#define PARTY_MON_SIZE 44u
#define BOX_MON_SIZE 33u
#define BOX_BLOCK_SIZE 0x462u

/* party block, relative to OFF_PARTY */
#define PARTY_COUNT 0x0000u
#define PARTY_SPECIES 0x0001u /* 6 entries + 0xFF */
#define PARTY_MONS 0x0008u
#define PARTY_NICKNAMES 0x0152u

/* box block, relative to a box base */
#define BOX_COUNT 0x0000u
#define BOX_SPECIES 0x0001u /* 20 entries + 0xFF */
#define BOX_MONS 0x0016u
#define BOX_NICKNAMES 0x0386u

/* mon record, relative to a mon base */
#define MON_SPECIES 0x00u
#define MON_CURRENT_HP 0x01u /* big-endian */
#define MON_BOX_LEVEL 0x03u
#define MON_STATUS 0x04u
#define MON_TYPE1 0x05u
#define MON_TYPE2 0x06u
#define MON_CATCH_RATE 0x07u
#define MON_MOVES 0x08u    /* 4 bytes */
#define MON_EXP 0x0Eu      /* 3 bytes big-endian */
#define MON_STAT_EXP 0x11u /* 5 x u16 big-endian */
#define MON_DVS 0x1Bu      /* 2 bytes */
#define MON_PP 0x1Du       /* 4 bytes: pp | ups << 6 */
/* party mon only */
#define PMON_LEVEL 0x21u
#define PMON_MAX_HP 0x22u
#define PMON_ATTACK 0x24u
#define PMON_DEFENSE 0x26u
#define PMON_SPEED 0x28u
#define PMON_SPECIAL 0x2Au

#define TEXT_TERM 0x50u
#define LIST_TERM 0xFFu

/* ---- module state ---- */

static uint8_t y_ready;
static uint8_t y_main_dirty;
static uint16_t y_box_dirty;  /* bit n = bank slot n edited */
static uint16_t y_status;     /* YE_F_* flags */
static uint16_t y_box_chk_ok; /* bit n = box n stored checksum ok */
static uint8_t y_current_box; /* 0..11 */
static uint8_t y_boxes_init;

/* ---- byte helpers ---- */

static uint16_t get16(uint16_t off) {
  return (uint16_t)(((uint16_t)save_get(off) << 8) |
                    save_get((uint16_t)(off + 1u)));
}

static void set16(uint16_t off, uint16_t v) {
  save_set(off, (uint8_t)(v >> 8));
  save_set((uint16_t)(off + 1u), (uint8_t)v);
}

static uint32_t get24(uint16_t off) {
  return ((uint32_t)save_get(off) << 16) |
         ((uint32_t)save_get((uint16_t)(off + 1u)) << 8) |
         (uint32_t)save_get((uint16_t)(off + 2u));
}

static void set24(uint16_t off, uint32_t v) {
  save_set(off, (uint8_t)(v >> 16));
  save_set((uint16_t)(off + 1u), (uint8_t)(v >> 8));
  save_set((uint16_t)(off + 2u), (uint8_t)v);
}

/* ---- location routing ---- */

static uint8_t loc_is_party(uint8_t loc) { return loc == YELLOW_LOC_PARTY; }

static uint16_t block_base(uint8_t loc) {
  if (loc_is_party(loc)) {
    return OFF_PARTY;
  }
  if ((uint8_t)(loc - 1u) == y_current_box) {
    return OFF_CURRENT_BOX;
  }
  if (loc < 7u) {
    return (uint16_t)(OFF_BANK2_BOXES + (uint16_t)(loc - 1u) * BOX_BLOCK_SIZE);
  }
  return (uint16_t)(OFF_BANK3_BOXES + (uint16_t)(loc - 7u) * BOX_BLOCK_SIZE);
}

static uint8_t loc_capacity(uint8_t loc) {
  return loc_is_party(loc) ? (uint8_t)YELLOW_PARTY_CAPACITY
                           : (uint8_t)YELLOW_BOX_CAPACITY;
}

static uint16_t mon_size(uint8_t loc) {
  return loc_is_party(loc) ? PARTY_MON_SIZE : BOX_MON_SIZE;
}

static uint16_t species_list_off(uint8_t loc) {
  return loc_is_party(loc) ? PARTY_SPECIES : BOX_SPECIES;
}

static uint16_t mon_base(uint8_t loc, uint8_t slot) {
  uint16_t mons = loc_is_party(loc) ? PARTY_MONS : BOX_MONS;
  return (uint16_t)(block_base(loc) + mons + (uint16_t)slot * mon_size(loc));
}

static uint16_t nick_base(uint8_t loc, uint8_t slot) {
  uint16_t nicks = loc_is_party(loc) ? PARTY_NICKNAMES : BOX_NICKNAMES;
  return (uint16_t)(block_base(loc) + nicks + (uint16_t)slot * NAME_LEN);
}

static void touch(uint8_t loc) {
  if (loc_is_party(loc) || (uint8_t)(loc - 1u) == y_current_box) {
    y_main_dirty = 1;
  } else {
    y_box_dirty |= (uint16_t)(1u << (loc - 1u));
  }
}

/* ---- checksums ---- */

/* Gen 1 checksum: 8-bit wrapping sum of [start, end], inverted. */
static uint8_t region_checksum(uint16_t start, uint16_t end) {
  uint8_t sum = 0;
  uint16_t off;

  for (off = start; off <= end; off++) {
    sum += save_get(off);
  }
  return (uint8_t)~sum;
}

static uint16_t bank_box_base(uint8_t n) {
  return n < 6u ? (uint16_t)(OFF_BANK2_BOXES + n * BOX_BLOCK_SIZE)
                : (uint16_t)(OFF_BANK3_BOXES + (n - 6u) * BOX_BLOCK_SIZE);
}

static uint16_t bank_box_checksum_off(uint8_t n) {
  return n < 6u ? (uint16_t)(OFF_BANK2_CHECKSUM + 1u + n)
                : (uint16_t)(OFF_BANK3_CHECKSUM + 1u + (n - 6u));
}

/* ---- stat math ---- */

static uint8_t ceil_sqrt_quotient(uint16_t stat_exp) {
  uint16_t r = 0;

  while ((uint32_t)r * r < stat_exp) {
    r++;
  }
  if (r > 255u) {
    r = 255u;
  }
  return (uint8_t)(r / 4u);
}

uint16_t yellow_calc_stat(uint8_t base, uint8_t dv, uint16_t stat_exp,
                          uint8_t level, uint8_t is_hp) YELLOW_BANKED {
  uint32_t core;

  core = ((uint32_t)base + dv) * 2u + ceil_sqrt_quotient(stat_exp);
  core = core * level / 100u;
  return (uint16_t)(core + (is_hp ? (uint32_t)level + 10u : 5u));
}

/* Growth rates from pokered GrowthRateTable. Medium-slow can go
 * negative at level 1 (true value -54); the game's 24-bit math wraps,
 * we clamp to 0. */
static uint32_t growth_exp(uint8_t growth_rate, uint8_t level) {
  uint32_t n = level;

  switch (growth_rate) {
  case 3: {
    int32_t t = (int32_t)(n * n * n * 6u / 5u) - (int32_t)(15u * n * n) +
                (int32_t)(100u * n) - 140;
    return t > 0 ? (uint32_t)t : 0u;
  }
  case 4:
    return n * n * n * 4u / 5u;
  case 5:
    return n * n * n * 5u / 4u;
  default: /* 0: medium fast */
    return n * n * n;
  }
}

uint32_t yellow_exp_for_level(uint8_t growth_rate,
                              uint8_t level) YELLOW_BANKED {
  if (level < 1u) {
    level = 1u;
  } else if (level > 100u) {
    level = 100u;
  }
  return growth_exp(growth_rate, level);
}

uint8_t yellow_level_for_exp(uint8_t growth_rate, uint32_t exp) YELLOW_BANKED {
  uint8_t level = 1u;

  if (growth_exp(growth_rate, 2u) > exp) {
    return 1u;
  }
  while (level < 100u &&
         growth_exp(growth_rate, (uint8_t)(level + 1u)) <= exp) {
    level++;
  }
  return level;
}

/* Max PP after `ups` PP Ups, as pokered AddBonusPP computes it: each
 * PP Up adds min(7, floor(base/5)); base 40 caps at 61, not 64. */
uint8_t yellow_max_pp(uint8_t base_pp, uint8_t ups) YELLOW_BANKED {
  uint8_t bonus = base_pp / 5u;

  if (bonus > 7u) {
    bonus = 7u;
  }
  if (ups > 3u) {
    ups = 3u;
  }
  return (uint8_t)(base_pp + ups * bonus);
}

/* ---- mon record helpers ---- */

/* Growth rate of the species stored in the record at `m`. */
static uint8_t mon_growth_rate(uint16_t m) {
  YellowBaseStats bs;

  yellow_base_stats(yellow_index_to_dex(save_get((uint16_t)(m + MON_SPECIES))),
                    &bs);
  return bs.growth_rate;
}

/* Recompute a party mon's five stored calculated stats from species,
 * level, DVs and stat exp, keeping current HP coherent: full stays
 * full, a deficit is preserved, a fainted mon (HP 0) stays fainted,
 * and the value only floors at 1 while alive. For a box mon (no
 * stored stats) current HP is just clamped to the new computed max. */
static void recalc_mon(uint8_t loc, uint8_t slot) {
  uint16_t m = mon_base(loc, slot);
  uint8_t species = save_get((uint16_t)(m + MON_SPECIES));
  YellowBaseStats bs;
  uint8_t dvs0 = save_get((uint16_t)(m + MON_DVS));
  uint8_t dvs1 = save_get((uint16_t)(m + MON_DVS + 1u));
  uint8_t atk_dv = (uint8_t)(dvs0 >> 4);
  uint8_t def_dv = (uint8_t)(dvs0 & 0xFu);
  uint8_t spd_dv = (uint8_t)(dvs1 >> 4);
  uint8_t spc_dv = (uint8_t)(dvs1 & 0xFu);
  uint8_t hp_dv = (uint8_t)(((atk_dv & 1u) << 3) | ((def_dv & 1u) << 2) |
                            ((spd_dv & 1u) << 1) | (spc_dv & 1u));
  uint8_t level;
  uint16_t new_max;
  uint16_t cur;

  yellow_base_stats(yellow_index_to_dex(species), &bs);
  if (loc_is_party(loc)) {
    level = save_get((uint16_t)(m + PMON_LEVEL));
  } else {
    level =
        yellow_level_for_exp(bs.growth_rate, get24((uint16_t)(m + MON_EXP)));
  }

  new_max = yellow_calc_stat(bs.hp, hp_dv, get16((uint16_t)(m + MON_STAT_EXP)),
                             level, 1u);
  cur = get16((uint16_t)(m + MON_CURRENT_HP));

  if (loc_is_party(loc)) {
    uint16_t old_max = get16((uint16_t)(m + PMON_MAX_HP));
    uint16_t deficit = (uint16_t)(old_max > cur ? old_max - cur : 0u);
    uint16_t new_cur;

    set16((uint16_t)(m + PMON_MAX_HP), new_max);
    set16((uint16_t)(m + PMON_ATTACK),
          yellow_calc_stat(bs.attack, atk_dv,
                           get16((uint16_t)(m + MON_STAT_EXP + 2u)), level,
                           0u));
    set16((uint16_t)(m + PMON_DEFENSE),
          yellow_calc_stat(bs.defense, def_dv,
                           get16((uint16_t)(m + MON_STAT_EXP + 4u)), level,
                           0u));
    set16((uint16_t)(m + PMON_SPEED),
          yellow_calc_stat(bs.speed, spd_dv,
                           get16((uint16_t)(m + MON_STAT_EXP + 6u)), level,
                           0u));
    set16((uint16_t)(m + PMON_SPECIAL),
          yellow_calc_stat(bs.special, spc_dv,
                           get16((uint16_t)(m + MON_STAT_EXP + 8u)), level,
                           0u));

    if (cur == 0u) {
      return; /* fainted: preserved */
    }
    new_cur = (uint16_t)(new_max > deficit ? new_max - deficit : 1u);
    set16((uint16_t)(m + MON_CURRENT_HP), new_cur);
  } else {
    if (cur > new_max) {
      set16((uint16_t)(m + MON_CURRENT_HP), new_max);
    }
  }
}

/* ---- validation ---- */

static uint8_t bcd3_valid(uint16_t off) {
  uint8_t i, b;

  for (i = 0; i < 3u; i++) {
    b = save_get((uint16_t)(off + i));
    if ((b & 0x0Fu) > 9u || (uint8_t)(b >> 4) > 9u) {
      return 0;
    }
  }
  return 1;
}

/* A terminated species list: count <= capacity, terminator present
 * right after the last entry, no zero (empty) entries in use. */
static uint8_t species_list_valid(uint16_t base, uint8_t capacity,
                                  uint16_t list_off) {
  uint8_t count = save_get(base);
  uint8_t i;

  if (count > capacity) {
    return 0;
  }
  if (save_get((uint16_t)(base + list_off + count)) != LIST_TERM) {
    return 0;
  }
  for (i = 0; i < count; i++) {
    if (save_get((uint16_t)(base + list_off + i)) == 0u) {
      return 0;
    }
  }
  return 1;
}

static uint8_t item_list_valid(uint16_t count_off, uint16_t items_off,
                               uint8_t capacity) {
  uint8_t count = save_get(count_off);
  uint8_t i;

  if (count > capacity) {
    return 0;
  }
  if (save_get((uint16_t)(items_off + 2u * count)) != LIST_TERM) {
    return 0;
  }
  for (i = 0; i < count; i++) {
    uint8_t item = save_get((uint16_t)(items_off + 2u * i));
    uint8_t quantity = save_get((uint16_t)(items_off + 2u * i + 1u));
    if (!yellow_item_valid(item) || quantity == 0u ||
        quantity > YELLOW_MAX_QTY) {
      return 0;
    }
  }
  return 1;
}

/* Species list entries must match their mon records (the parallel
 * array invariant the game relies on), and each live mon must have a
 * species with a Dex entry and a nonempty move set. */
static uint8_t mon_list_consistent(uint16_t base, uint8_t capacity,
                                   uint16_t list_off, uint16_t mons_off,
                                   uint16_t mon_size_) {
  uint8_t count = save_get(base);
  uint8_t i;

  if (count > capacity) {
    return 0;
  }
  for (i = 0; i < count; i++) {
    uint16_t m = (uint16_t)(base + mons_off + (uint16_t)i * mon_size_);
    uint8_t species = save_get((uint16_t)(m + MON_SPECIES));
    uint8_t j;

    if (species != save_get((uint16_t)(base + list_off + i))) {
      return 0;
    }
    if (yellow_index_to_dex(species) == 0u) {
      return 0;
    }
    if (mon_size_ == PARTY_MON_SIZE &&
        (save_get((uint16_t)(m + PMON_LEVEL)) < YELLOW_MIN_LEVEL ||
         save_get((uint16_t)(m + PMON_LEVEL)) > YELLOW_MAX_LEVEL)) {
      return 0;
    }
    /* moves: contiguous, first nonempty */
    if (save_get((uint16_t)(m + MON_MOVES)) == 0u) {
      return 0;
    }
    for (j = 0; j < 4u; j++) {
      uint8_t mv = save_get((uint16_t)(m + MON_MOVES + j));
      if (mv != 0u && !yellow_move_valid(mv)) {
        return 0;
      }
    }
    for (j = 1; j < 4u; j++) {
      if (save_get((uint16_t)(m + MON_MOVES + j - 1u)) == 0u &&
          save_get((uint16_t)(m + MON_MOVES + j)) != 0u) {
        return 0; /* hole in the move list */
      }
    }
  }
  return 1;
}

YellowError yellow_init(void) YELLOW_BANKED {
  uint8_t boxbyte = save_get(OFF_CURRENT_BOX_NUM);
  uint8_t n;

  y_ready = 0;
  y_main_dirty = 0;
  y_box_dirty = 0;
  y_status = 0;
  y_box_chk_ok = 0;

  if (save_get(OFF_MAIN_CHECKSUM) !=
      region_checksum(OFF_MAIN_CHK_START, OFF_MAIN_CHK_END)) {
    y_status |= YE_F_MAIN_CHECKSUM;
  }

  y_boxes_init = (uint8_t)((boxbyte & 0x80u) != 0u);
  y_current_box = (uint8_t)(boxbyte & 0x7Fu);
  if (!y_boxes_init) {
    y_status |= YE_F_BOXES_UNINITIALIZED;
  } else {
    if (save_get(OFF_BANK2_CHECKSUM) !=
        region_checksum(OFF_BANK2_BOXES, (uint16_t)(OFF_BANK2_CHECKSUM - 1u))) {
      y_status |= YE_F_BANK2_CHECKSUM;
    }
    if (save_get(OFF_BANK3_CHECKSUM) !=
        region_checksum(OFF_BANK3_BOXES, (uint16_t)(OFF_BANK3_CHECKSUM - 1u))) {
      y_status |= YE_F_BANK3_CHECKSUM;
    }
    for (n = 0; n < 12u; n++) {
      uint16_t base = bank_box_base(n);
      if (save_get(bank_box_checksum_off(n)) ==
          region_checksum(base, (uint16_t)(base + BOX_BLOCK_SIZE - 1u))) {
        y_box_chk_ok |= (uint16_t)(1u << n);
      } else {
        y_status |= YE_F_BOX_CHECKSUM;
      }
    }
  }
  if (y_current_box >= YELLOW_NUM_BOXES) {
    return YE_ERR_MALFORMED_SAVE;
  }

  /* party */
  if (!species_list_valid(OFF_PARTY, (uint8_t)YELLOW_PARTY_CAPACITY,
                          PARTY_SPECIES) ||
      !mon_list_consistent(OFF_PARTY, (uint8_t)YELLOW_PARTY_CAPACITY,
                           PARTY_SPECIES, PARTY_MONS, PARTY_MON_SIZE)) {
    return YE_ERR_MALFORMED_SAVE;
  }

  /* current-box working copy: always validated and accessible */
  if (!species_list_valid(OFF_CURRENT_BOX, (uint8_t)YELLOW_BOX_CAPACITY,
                          BOX_SPECIES) ||
      !mon_list_consistent(OFF_CURRENT_BOX, (uint8_t)YELLOW_BOX_CAPACITY,
                           BOX_SPECIES, BOX_MONS, BOX_MON_SIZE)) {
    return YE_ERR_MALFORMED_SAVE;
  }

  /* bank box slots: validated only when the game initialized them;
   * otherwise they hold erased/garbage bytes the game will wipe */
  if (y_boxes_init) {
    for (n = 0; n < 12u; n++) {
      uint16_t base = bank_box_base(n);
      if (!species_list_valid(base, (uint8_t)YELLOW_BOX_CAPACITY,
                              BOX_SPECIES) ||
          !mon_list_consistent(base, (uint8_t)YELLOW_BOX_CAPACITY, BOX_SPECIES,
                               BOX_MONS, BOX_MON_SIZE)) {
        return YE_ERR_MALFORMED_SAVE;
      }
    }
  }

  /* bag, money */
  if (!item_list_valid(OFF_BAG_COUNT, OFF_BAG_ITEMS,
                       (uint8_t)YELLOW_BAG_CAPACITY)) {
    return YE_ERR_MALFORMED_SAVE;
  }
  if (!bcd3_valid(OFF_MONEY)) {
    return YE_ERR_MALFORMED_SAVE;
  }

  y_ready = 1;
  return YE_OK;
}

uint16_t yellow_status_flags(void) YELLOW_BANKED { return y_status; }

uint8_t yellow_box_checksum_ok(uint8_t n) YELLOW_BANKED {
  if (n >= 12u) {
    return 0;
  }
  return (uint8_t)((y_box_chk_ok & (1u << n)) != 0u);
}

uint8_t yellow_is_yellow_hint(void) YELLOW_BANKED {
  if (save_get(OFF_PLAYER_STARTER) == YELLOW_PIKACHU_INDEX) {
    return 2u;
  }
  if (save_get(OFF_PLAYER_STARTER) == 0u &&
      save_get(OFF_PIKACHU_FRIENDSHIP) != 0u) {
    return 1u;
  }
  return 0u;
}

uint8_t yellow_main_dirty(void) YELLOW_BANKED { return y_main_dirty; }

uint16_t yellow_dirty_box_mask(void) YELLOW_BANKED { return y_box_dirty; }

uint8_t yellow_is_dirty(void) YELLOW_BANKED {
  return (uint8_t)(y_main_dirty != 0u || y_box_dirty != 0u);
}

void yellow_flush(void) YELLOW_BANKED {
  uint8_t n;

  if (!y_ready) {
    return;
  }
  if (y_main_dirty) {
    save_set(OFF_MAIN_CHECKSUM,
             region_checksum(OFF_MAIN_CHK_START, OFF_MAIN_CHK_END));
  }
  if ((y_box_dirty & 0x003Fu) != 0u) {
    save_set(
        OFF_BANK2_CHECKSUM,
        region_checksum(OFF_BANK2_BOXES, (uint16_t)(OFF_BANK2_CHECKSUM - 1u)));
  }
  if ((y_box_dirty & 0x0FC0u) != 0u) {
    save_set(
        OFF_BANK3_CHECKSUM,
        region_checksum(OFF_BANK3_BOXES, (uint16_t)(OFF_BANK3_CHECKSUM - 1u)));
  }
  for (n = 0; n < 12u; n++) {
    if ((y_box_dirty & (1u << n)) != 0u) {
      uint16_t base = bank_box_base(n);
      save_set(bank_box_checksum_off(n),
               region_checksum(base, (uint16_t)(base + BOX_BLOCK_SIZE - 1u)));
    }
  }
}

void yellow_mark_saved(void) YELLOW_BANKED {
  y_main_dirty = 0;
  y_box_dirty = 0;
}

void yellow_error_string(YellowError err, char *buf,
                         uint8_t buflen) YELLOW_BANKED {
  const char *s;
  uint8_t i;

  switch (err) {
  case YE_OK:
    s = "OK";
    break;
  case YE_ERR_NOT_INITIALIZED:
    s = "NOT READY";
    break;
  case YE_ERR_MALFORMED_SAVE:
    s = "CORRUPT SAVE";
    break;
  case YE_ERR_BAD_LOCATION:
    s = "BAD LOCATION";
    break;
  case YE_ERR_BAD_SLOT:
    s = "BAD SLOT";
    break;
  case YE_ERR_EMPTY_SLOT:
    s = "EMPTY SLOT";
    break;
  case YE_ERR_BAD_SPECIES:
    s = "BAD SPECIES";
    break;
  case YE_ERR_BAD_LEVEL:
    s = "BAD LEVEL";
    break;
  case YE_ERR_BAD_MOVE:
    s = "BAD MOVE";
    break;
  case YE_ERR_BAD_MOVE_INDEX:
    s = "BAD MOVE SLOT";
    break;
  case YE_ERR_BAD_STAT:
    s = "BAD STAT";
    break;
  case YE_ERR_BAD_DV:
    s = "BAD DV";
    break;
  case YE_ERR_BAD_BAG_INDEX:
    s = "BAD BAG SLOT";
    break;
  case YE_ERR_BAG_FULL:
    s = "BAG FULL";
    break;
  case YE_ERR_BAD_ITEM:
    s = "BAD ITEM";
    break;
  case YE_ERR_MONEY_RANGE:
    s = "MONEY RANGE";
    break;
  case YE_ERR_TEXT_UNENCODABLE:
    s = "BAD CHARACTER";
    break;
  case YE_ERR_TEXT_TOO_LONG:
    s = "NAME TOO LONG";
    break;
  case YE_ERR_BOXES_UNINITIALIZED:
    s = "BOXES WIPED";
    break;
  case YE_ERR_BAD_PP_UPS:
    s = "BAD PP UPS";
    break;
  case YE_ERR_BAD_BADGE:
    s = "BAD BADGE";
    break;
  case YE_ERR_MOVE_WIPE_ALL:
    s = "NEEDS A MOVE";
    break;
  default:
    s = "ERROR";
    break;
  }

  if (buflen == 0u) {
    return;
  }
  for (i = 0; i + 1u < buflen && s[i] != '\0'; i++) {
    buf[i] = s[i];
  }
  buf[i] = '\0';
}

/* ---- counts and locations ---- */

uint8_t yellow_party_count(void) YELLOW_BANKED {
  if (!y_ready) {
    return 0;
  }
  return save_get(OFF_PARTY);
}

uint8_t yellow_box_count(uint8_t n) YELLOW_BANKED {
  if (!y_ready || n >= 12u) {
    return 0;
  }
  if (n == y_current_box) {
    return save_get(OFF_CURRENT_BOX);
  }
  if (!y_boxes_init) {
    return 0;
  }
  return save_get(bank_box_base(n));
}

uint8_t yellow_current_box(void) YELLOW_BANKED { return y_current_box; }

uint8_t yellow_boxes_initialized(void) YELLOW_BANKED { return y_boxes_init; }

/* Common edit precondition: core ready, valid location, slot within
 * capacity and holding an existing Pokemon; bank box slots require
 * initialized boxes. */
static YellowError check_edit(uint8_t loc, uint8_t slot) {
  if (!y_ready) {
    return YE_ERR_NOT_INITIALIZED;
  }
  if (loc > YELLOW_NUM_BOXES) {
    return YE_ERR_BAD_LOCATION;
  }
  if (slot >= loc_capacity(loc)) {
    return YE_ERR_BAD_SLOT;
  }
  if (loc_is_party(loc)) {
    if (slot >= save_get(OFF_PARTY)) {
      return YE_ERR_EMPTY_SLOT;
    }
  } else {
    if (!y_boxes_init && (uint8_t)(loc - 1u) != y_current_box) {
      return YE_ERR_BOXES_UNINITIALIZED;
    }
    if (slot >= save_get(block_base(loc))) {
      return YE_ERR_EMPTY_SLOT;
    }
  }
  return YE_OK;
}

/* ---- reading Pokemon ---- */

void yellow_get_pokemon(uint8_t loc, uint8_t slot,
                        YellowPokemonView *out) YELLOW_BANKED {
  uint16_t m;
  uint8_t i;
  uint8_t count;
  uint8_t species;

  out->valid = 0;
  out->species = 0;
  out->dex = 0;
  out->level = 0;
  out->exp = 0;
  out->types[0] = 0;
  out->types[1] = 0;
  out->catch_rate = 0;
  out->current_hp = 0;
  out->status = 0;
  out->move_count = 0;
  out->max_hp = 0;
  out->attack = 0;
  out->defense = 0;
  out->speed = 0;
  out->special = 0;
  for (i = 0; i < 4u; i++) {
    out->moves[i] = 0;
    out->pp[i] = 0;
  }
  out->dvs[0] = 0;
  out->dvs[1] = 0;
  for (i = 0; i < 5u; i++) {
    out->stat_exp[i] = 0;
  }

  if (!y_ready || loc > YELLOW_NUM_BOXES || slot >= loc_capacity(loc)) {
    return;
  }
  count = loc_is_party(loc) ? save_get(OFF_PARTY)
                            : yellow_box_count((uint8_t)(loc - 1u));
  if (slot >= count) {
    return;
  }

  m = mon_base(loc, slot);
  species = save_get((uint16_t)(m + MON_SPECIES));
  out->valid = 1;
  out->species = species;
  out->dex = yellow_index_to_dex(species);
  out->exp = get24((uint16_t)(m + MON_EXP));
  out->types[0] = save_get((uint16_t)(m + MON_TYPE1));
  out->types[1] = save_get((uint16_t)(m + MON_TYPE2));
  out->catch_rate = save_get((uint16_t)(m + MON_CATCH_RATE));
  out->current_hp = get16((uint16_t)(m + MON_CURRENT_HP));
  out->status = save_get((uint16_t)(m + MON_STATUS));
  for (i = 0; i < 4u; i++) {
    out->moves[i] = save_get((uint16_t)(m + MON_MOVES + i));
    out->pp[i] = save_get((uint16_t)(m + MON_PP + i));
  }
  for (i = 0; i < 4u && out->moves[i] != 0u; i++) {
    out->move_count++;
  }
  out->dvs[0] = save_get((uint16_t)(m + MON_DVS));
  out->dvs[1] = save_get((uint16_t)(m + MON_DVS + 1u));
  for (i = 0; i < 5u; i++) {
    out->stat_exp[i] = get16((uint16_t)(m + MON_STAT_EXP + 2u * i));
  }

  if (loc_is_party(loc)) {
    out->level = save_get((uint16_t)(m + PMON_LEVEL));
    out->max_hp = get16((uint16_t)(m + PMON_MAX_HP));
    out->attack = get16((uint16_t)(m + PMON_ATTACK));
    out->defense = get16((uint16_t)(m + PMON_DEFENSE));
    out->speed = get16((uint16_t)(m + PMON_SPEED));
    out->special = get16((uint16_t)(m + PMON_SPECIAL));
  } else {
    YellowBaseStats bs;
    uint8_t hp_dv =
        (uint8_t)((((out->dvs[0] >> 4) & 1u) << 3) | ((out->dvs[0] & 1u) << 2) |
                  (((out->dvs[1] >> 4) & 1u) << 1) | (out->dvs[1] & 1u));
    yellow_base_stats(out->dex, &bs);
    out->level = yellow_level_for_exp(bs.growth_rate, out->exp);
    out->max_hp =
        yellow_calc_stat(bs.hp, hp_dv, out->stat_exp[0], out->level, 1u);
    out->attack = yellow_calc_stat(bs.attack, (uint8_t)(out->dvs[0] >> 4),
                                   out->stat_exp[1], out->level, 0u);
    out->defense = yellow_calc_stat(bs.defense, (uint8_t)(out->dvs[0] & 0xFu),
                                    out->stat_exp[2], out->level, 0u);
    out->speed = yellow_calc_stat(bs.speed, (uint8_t)(out->dvs[1] >> 4),
                                  out->stat_exp[3], out->level, 0u);
    out->special = yellow_calc_stat(bs.special, (uint8_t)(out->dvs[1] & 0xFu),
                                    out->stat_exp[4], out->level, 0u);
  }
}

void yellow_get_nickname(uint8_t loc, uint8_t slot,
                         uint8_t *out) YELLOW_BANKED {
  uint8_t i;
  uint8_t count;

  for (i = 0; i < NAME_LEN; i++) {
    out[i] = TEXT_TERM;
  }
  if (!y_ready || loc > YELLOW_NUM_BOXES || slot >= loc_capacity(loc)) {
    return;
  }
  count = loc_is_party(loc) ? save_get(OFF_PARTY)
                            : yellow_box_count((uint8_t)(loc - 1u));
  if (slot >= count) {
    return;
  }
  for (i = 0; i < NAME_LEN; i++) {
    out[i] = save_get((uint16_t)(nick_base(loc, slot) + i));
  }
}

/* ---- editing Pokemon ---- */

YellowError yellow_set_species(uint8_t loc, uint8_t slot,
                               uint8_t species_index) YELLOW_BANKED {
  YellowError e = check_edit(loc, slot);
  YellowBaseStats bs;
  uint16_t m;
  uint8_t dex;
  uint8_t level;

  if (e != YE_OK) {
    return e;
  }
  dex = yellow_index_to_dex(species_index);
  if (dex == 0u) {
    return YE_ERR_BAD_SPECIES;
  }

  m = mon_base(loc, slot);
  level = loc_is_party(loc)
              ? save_get((uint16_t)(m + PMON_LEVEL))
              : yellow_level_for_exp(mon_growth_rate(m),
                                     get24((uint16_t)(m + MON_EXP)));

  save_set((uint16_t)(m + MON_SPECIES), species_index);
  save_set((uint16_t)(block_base(loc) + species_list_off(loc) + slot),
           species_index);
  yellow_base_stats(dex, &bs);
  save_set((uint16_t)(m + MON_TYPE1), bs.type1);
  save_set((uint16_t)(m + MON_TYPE2), bs.type2);
  save_set((uint16_t)(m + MON_CATCH_RATE), bs.catch_rate);

  set24((uint16_t)(m + MON_EXP), yellow_exp_for_level(bs.growth_rate, level));
  save_set((uint16_t)(m + MON_BOX_LEVEL), level);
  if (loc_is_party(loc)) {
    save_set((uint16_t)(m + PMON_LEVEL), level);
  }
  recalc_mon(loc, slot);
  touch(loc);
  return YE_OK;
}

YellowError yellow_set_level(uint8_t loc, uint8_t slot,
                             uint8_t level) YELLOW_BANKED {
  YellowError e = check_edit(loc, slot);
  uint16_t m;
  uint8_t growth;

  if (e != YE_OK) {
    return e;
  }
  if (level < YELLOW_MIN_LEVEL || level > YELLOW_MAX_LEVEL) {
    return YE_ERR_BAD_LEVEL;
  }
  m = mon_base(loc, slot);
  growth = mon_growth_rate(m);
  set24((uint16_t)(m + MON_EXP), yellow_exp_for_level(growth, level));
  save_set((uint16_t)(m + MON_BOX_LEVEL), level);
  if (loc_is_party(loc)) {
    save_set((uint16_t)(m + PMON_LEVEL), level);
  }
  recalc_mon(loc, slot);
  touch(loc);
  return YE_OK;
}

YellowError yellow_set_exp(uint8_t loc, uint8_t slot,
                           uint32_t exp) YELLOW_BANKED {
  YellowError e = check_edit(loc, slot);
  uint16_t m;
  uint8_t growth;
  uint8_t level;

  if (e != YE_OK) {
    return e;
  }
  if (exp > YELLOW_MAX_EXP) {
    return YE_ERR_BAD_LEVEL;
  }
  m = mon_base(loc, slot);
  growth = mon_growth_rate(m);
  level = yellow_level_for_exp(growth, exp);
  set24((uint16_t)(m + MON_EXP), exp);
  save_set((uint16_t)(m + MON_BOX_LEVEL), level);
  if (loc_is_party(loc)) {
    save_set((uint16_t)(m + PMON_LEVEL), level);
  }
  recalc_mon(loc, slot);
  touch(loc);
  return YE_OK;
}

YellowError yellow_set_move(uint8_t loc, uint8_t slot, uint8_t move_index,
                            uint8_t move_id) YELLOW_BANKED {
  YellowError e = check_edit(loc, slot);
  uint16_t m;
  uint8_t moves[4];
  uint8_t pps[4];
  uint8_t i;

  if (e != YE_OK) {
    return e;
  }
  if (move_index >= YELLOW_MOVE_SLOTS) {
    return YE_ERR_BAD_MOVE_INDEX;
  }
  if (move_id != 0u && !yellow_move_valid(move_id)) {
    return YE_ERR_BAD_MOVE;
  }

  m = mon_base(loc, slot);
  for (i = 0; i < 4u; i++) {
    moves[i] = save_get((uint16_t)(m + MON_MOVES + i));
    pps[i] = save_get((uint16_t)(m + MON_PP + i));
  }

  if (move_id == 0u) {
    /* erase: shift later moves down, keep their PP bytes */
    for (i = move_index; i + 1u < 4u; i++) {
      moves[i] = moves[i + 1u];
      pps[i] = pps[i + 1u];
    }
    moves[3] = 0u;
    pps[3] = 0u;
  } else {
    /* write: if the target slot is beyond the live moves, or any
     * earlier slot is empty, place the move in the first empty
     * slot so the list stays contiguous (no holes) */
    uint8_t target = move_index;
    for (i = 0; i < move_index; i++) {
      if (moves[i] == 0u) {
        target = i;
        break;
      }
    }
    moves[target] = move_id;
    pps[target] = yellow_move_base_pp(move_id); /* fresh slot */
  }

  if (moves[0] == 0u) {
    return YE_ERR_MOVE_WIPE_ALL; /* would leave no moves */
  }

  for (i = 0; i < 4u; i++) {
    save_set((uint16_t)(m + MON_MOVES + i), moves[i]);
    save_set((uint16_t)(m + MON_PP + i), pps[i]);
  }
  touch(loc);
  return YE_OK;
}

YellowError yellow_set_move_pp(uint8_t loc, uint8_t slot, uint8_t move_index,
                               uint8_t pp) YELLOW_BANKED {
  YellowError e = check_edit(loc, slot);
  uint16_t m;
  uint8_t move_id;
  uint8_t ups;
  uint8_t max_pp;

  if (e != YE_OK) {
    return e;
  }
  if (move_index >= YELLOW_MOVE_SLOTS) {
    return YE_ERR_BAD_MOVE_INDEX;
  }
  m = mon_base(loc, slot);
  move_id = save_get((uint16_t)(m + MON_MOVES + move_index));
  if (!yellow_move_valid(move_id)) {
    return YE_ERR_BAD_MOVE; /* empty slot: no PP to set */
  }
  ups = (uint8_t)(save_get((uint16_t)(m + MON_PP + move_index)) >> 6);
  max_pp = yellow_max_pp(yellow_move_base_pp(move_id), ups);
  if (pp > max_pp) {
    pp = max_pp;
  }
  save_set((uint16_t)(m + MON_PP + move_index),
           (uint8_t)((uint8_t)(ups << 6) | (pp & 0x3Fu)));
  touch(loc);
  return YE_OK;
}

YellowError yellow_set_pp_ups(uint8_t loc, uint8_t slot, uint8_t move_index,
                              uint8_t ups) YELLOW_BANKED {
  YellowError e = check_edit(loc, slot);
  uint16_t m;
  uint8_t move_id;
  uint8_t pp;
  uint8_t max_pp;

  if (e != YE_OK) {
    return e;
  }
  if (move_index >= YELLOW_MOVE_SLOTS) {
    return YE_ERR_BAD_MOVE_INDEX;
  }
  if (ups > YELLOW_MAX_PP_UPS) {
    return YE_ERR_BAD_PP_UPS;
  }
  m = mon_base(loc, slot);
  move_id = save_get((uint16_t)(m + MON_MOVES + move_index));
  if (!yellow_move_valid(move_id)) {
    return YE_ERR_BAD_MOVE; /* empty slot: no PP Ups */
  }
  pp = (uint8_t)(save_get((uint16_t)(m + MON_PP + move_index)) & 0x3Fu);
  max_pp = yellow_max_pp(yellow_move_base_pp(move_id), ups);
  if (pp > max_pp) {
    pp = max_pp;
  }
  save_set((uint16_t)(m + MON_PP + move_index),
           (uint8_t)((uint8_t)(ups << 6) | (pp & 0x3Fu)));
  touch(loc);
  return YE_OK;
}

YellowError yellow_set_dvs(uint8_t loc, uint8_t slot, uint8_t attack,
                           uint8_t defense, uint8_t speed,
                           uint8_t special) YELLOW_BANKED {
  YellowError e = check_edit(loc, slot);
  uint16_t m;

  if (e != YE_OK) {
    return e;
  }
  if (attack > YELLOW_MAX_DV || defense > YELLOW_MAX_DV ||
      speed > YELLOW_MAX_DV || special > YELLOW_MAX_DV) {
    return YE_ERR_BAD_DV;
  }
  m = mon_base(loc, slot);
  save_set((uint16_t)(m + MON_DVS), (uint8_t)((attack << 4) | defense));
  save_set((uint16_t)(m + MON_DVS + 1u), (uint8_t)((speed << 4) | special));
  recalc_mon(loc, slot);
  touch(loc);
  return YE_OK;
}

YellowError yellow_set_stat_exp(uint8_t loc, uint8_t slot, YellowStat stat,
                                uint16_t value) YELLOW_BANKED {
  YellowError e = check_edit(loc, slot);
  uint16_t m;

  if (e != YE_OK) {
    return e;
  }
  if ((uint8_t)stat >= YELLOW_STAT_COUNT) {
    return YE_ERR_BAD_STAT;
  }
  m = mon_base(loc, slot);
  set16((uint16_t)(m + MON_STAT_EXP + 2u * (uint8_t)stat), value);
  recalc_mon(loc, slot);
  touch(loc);
  return YE_OK;
}

YellowError yellow_recalculate_stats(uint8_t loc, uint8_t slot) YELLOW_BANKED {
  YellowError e = check_edit(loc, slot);

  if (e != YE_OK) {
    return e;
  }
  if (loc_is_party(loc)) {
    recalc_mon(loc, slot);
    touch(loc);
  }
  return YE_OK;
}

YellowError yellow_set_nickname(uint8_t loc, uint8_t slot,
                                const uint8_t *raw) YELLOW_BANKED {
  YellowError e = check_edit(loc, slot);
  uint16_t n = nick_base(loc, slot);
  uint8_t i;
  uint8_t term = 0;

  if (e != YE_OK) {
    return e;
  }
  for (i = 0; i < NAME_LEN && raw[i] != TEXT_TERM; ++i) {
  }
  if (i == NAME_LEN)
    return YE_ERR_TEXT_TOO_LONG;
  for (i = 0; i < NAME_LEN; i++) {
    if (raw[i] == TEXT_TERM) {
      term = 1;
    }
    save_set((uint16_t)(n + i), term ? TEXT_TERM : raw[i]);
  }
  touch(loc);
  return YE_OK;
}

/* ---- trainer ---- */

uint32_t yellow_get_money(void) YELLOW_BANKED {
  uint32_t value = 0;
  uint8_t i;

  if (!y_ready) {
    return 0;
  }
  for (i = 0; i < 3u; i++) {
    uint8_t b = save_get((uint16_t)(OFF_MONEY + i));
    value = value * 100u + (uint32_t)(b >> 4) * 10u + (uint32_t)(b & 0x0Fu);
  }
  return value;
}

YellowError yellow_set_money(uint32_t value) YELLOW_BANKED {
  uint8_t bytes[3];
  uint8_t i;

  if (!y_ready) {
    return YE_ERR_NOT_INITIALIZED;
  }
  if (value > YELLOW_MAX_MONEY) {
    return YE_ERR_MONEY_RANGE;
  }
  /* big-endian packed BCD, zero-padded on the left */
  for (i = 3u; i-- > 0u;) {
    bytes[i] = (uint8_t)(((value % 10u) & 0x0Fu) |
                         (uint8_t)(((value / 10u) % 10u) << 4));
    value /= 100u;
  }
  for (i = 0; i < 3u; i++) {
    save_set((uint16_t)(OFF_MONEY + i), bytes[i]);
  }
  y_main_dirty = 1;
  return YE_OK;
}

uint8_t yellow_get_badges(void) YELLOW_BANKED {
  if (!y_ready) {
    return 0;
  }
  return save_get(OFF_BADGES);
}

void yellow_set_badges(uint8_t badges) YELLOW_BANKED {
  if (!y_ready) {
    return;
  }
  save_set(OFF_BADGES, badges);
  y_main_dirty = 1;
}

uint8_t yellow_has_badge(YellowBadge badge) YELLOW_BANKED {
  if (!y_ready || (uint8_t)badge >= YELLOW_BADGE_COUNT) {
    return 0;
  }
  return (uint8_t)((save_get(OFF_BADGES) & (1u << (uint8_t)badge)) != 0u);
}

void yellow_set_badge(YellowBadge badge, uint8_t obtained) YELLOW_BANKED {
  uint8_t b;

  if (!y_ready || (uint8_t)badge >= YELLOW_BADGE_COUNT) {
    return;
  }
  b = save_get(OFF_BADGES);
  if (obtained) {
    b |= (uint8_t)(1u << (uint8_t)badge);
  } else {
    b &= (uint8_t)~(uint8_t)(1u << (uint8_t)badge);
  }
  save_set(OFF_BADGES, b);
  y_main_dirty = 1;
}

uint8_t yellow_pikachu_friendship(void) YELLOW_BANKED {
  if (!y_ready) {
    return 0;
  }
  return save_get(OFF_PIKACHU_FRIENDSHIP);
}

void yellow_set_pikachu_friendship(uint8_t value) YELLOW_BANKED {
  if (!y_ready) {
    return;
  }
  save_set(OFF_PIKACHU_FRIENDSHIP, value);
  y_main_dirty = 1;
}

/* ---- bag ---- */

static uint8_t bag_count_raw(void) { return save_get(OFF_BAG_COUNT); }

uint8_t yellow_bag_count(void) YELLOW_BANKED {
  if (!y_ready) {
    return 0;
  }
  return bag_count_raw();
}

void yellow_bag_get(uint8_t index, uint8_t *item_id,
                    uint8_t *qty) YELLOW_BANKED {
  *item_id = 0;
  *qty = 0;
  if (!y_ready || index >= bag_count_raw()) {
    return;
  }
  *item_id = save_get((uint16_t)(OFF_BAG_ITEMS + 2u * index));
  *qty = save_get((uint16_t)(OFF_BAG_ITEMS + 2u * index + 1u));
}

YellowError yellow_bag_set_item(uint8_t index, uint8_t item_id) YELLOW_BANKED {
  if (!y_ready) {
    return YE_ERR_NOT_INITIALIZED;
  }
  if (index >= bag_count_raw()) {
    return YE_ERR_BAD_BAG_INDEX;
  }
  if (!yellow_item_valid(item_id)) {
    return YE_ERR_BAD_ITEM;
  }
  save_set((uint16_t)(OFF_BAG_ITEMS + 2u * index), item_id);
  y_main_dirty = 1;
  return YE_OK;
}

YellowError yellow_bag_set_qty(uint8_t index, uint8_t qty) YELLOW_BANKED {
  if (!y_ready) {
    return YE_ERR_NOT_INITIALIZED;
  }
  if (index >= bag_count_raw()) {
    return YE_ERR_BAD_BAG_INDEX;
  }
  if (qty < 1u) {
    qty = 1u;
  } else if (qty > YELLOW_MAX_QTY) {
    qty = YELLOW_MAX_QTY;
  }
  save_set((uint16_t)(OFF_BAG_ITEMS + 2u * index + 1u), qty);
  y_main_dirty = 1;
  return YE_OK;
}

YellowError yellow_bag_add(uint8_t item_id, uint8_t qty) YELLOW_BANKED {
  uint8_t len;

  if (!y_ready) {
    return YE_ERR_NOT_INITIALIZED;
  }
  if (!yellow_item_valid(item_id)) {
    return YE_ERR_BAD_ITEM;
  }
  len = bag_count_raw();
  if (len >= YELLOW_BAG_CAPACITY) {
    return YE_ERR_BAG_FULL;
  }
  if (qty < 1u) {
    qty = 1u;
  } else if (qty > YELLOW_MAX_QTY) {
    qty = YELLOW_MAX_QTY;
  }
  save_set((uint16_t)(OFF_BAG_ITEMS + 2u * len), item_id);
  save_set((uint16_t)(OFF_BAG_ITEMS + 2u * len + 1u), qty);
  save_set((uint16_t)(OFF_BAG_ITEMS + 2u * len + 2u), LIST_TERM);
  save_set(OFF_BAG_COUNT, (uint8_t)(len + 1u));
  y_main_dirty = 1;
  return YE_OK;
}

YellowError yellow_bag_remove(uint8_t index) YELLOW_BANKED {
  uint8_t len;
  uint8_t i;

  if (!y_ready) {
    return YE_ERR_NOT_INITIALIZED;
  }
  len = bag_count_raw();
  if (index >= len) {
    return YE_ERR_BAD_BAG_INDEX;
  }
  for (i = index; i + 1u < len; i++) {
    save_set((uint16_t)(OFF_BAG_ITEMS + 2u * i),
             save_get((uint16_t)(OFF_BAG_ITEMS + 2u * (i + 1u))));
    save_set((uint16_t)(OFF_BAG_ITEMS + 2u * i + 1u),
             save_get((uint16_t)(OFF_BAG_ITEMS + 2u * (i + 1u) + 1u)));
  }
  save_set((uint16_t)(OFF_BAG_ITEMS + 2u * (len - 1u)), LIST_TERM);
  save_set(OFF_BAG_COUNT, (uint8_t)(len - 1u));
  y_main_dirty = 1;
  return YE_OK;
}

/* ---- text codec ---- */

char yellow_decode_byte(uint8_t raw) YELLOW_BANKED {
  if (raw == TEXT_TERM) {
    return 0;
  }
  if (raw >= 0x80u && raw <= 0x99u) {
    return (char)('A' + (raw - 0x80u));
  }
  if (raw >= 0xA0u && raw <= 0xB9u) {
    return (char)('a' + (raw - 0xA0u));
  }
  if (raw >= 0xF6u) {
    return (char)('0' + (raw - 0xF6u));
  }
  switch (raw) {
  case 0x54u:
    return '#'; /* the POKe macro glyph */
  case 0x7Fu:
    return ' ';
  case 0x9Au:
    return '(';
  case 0x9Bu:
    return ')';
  case 0x9Cu:
    return ':';
  case 0x9Du:
    return ';';
  case 0x9Eu:
    return '[';
  case 0x9Fu:
    return ']';
  case 0xBAu:
    return 'e'; /* e-acute */
  case 0xBBu:
    return 'd'; /* 'd ligature */
  case 0xBCu:
    return 'l';
  case 0xBDu:
    return 's';
  case 0xBEu:
    return 't';
  case 0xBFu:
    return 'v';
  case 0xE0u:
    return '\'';
  case 0xE3u:
    return '-';
  case 0xE4u:
    return 'r';
  case 0xE5u:
    return 'm';
  case 0xE6u:
    return '?';
  case 0xE7u:
    return '!';
  case 0xE8u:
    return '.';
  case 0xEFu:
    return 'M'; /* male */
  case 0xF3u:
    return '/';
  case 0xF4u:
    return ',';
  case 0xF5u:
    return 'F'; /* female */
  default:
    return '?';
  }
}

/* ASCII -> Gen 1 byte; 0 = no mapping. */
static uint8_t encode_char(char c) {
  if (c >= 'A' && c <= 'Z') {
    return (uint8_t)(0x80u + (c - 'A'));
  }
  if (c >= 'a' && c <= 'z') {
    return (uint8_t)(0xA0u + (c - 'a'));
  }
  if (c >= '0' && c <= '9') {
    return (uint8_t)(0xF6u + (c - '0'));
  }
  switch (c) {
  case ' ':
    return 0x7Fu;
  case '-':
    return 0xE3u;
  case '?':
    return 0xE6u;
  case '!':
    return 0xE7u;
  case '.':
    return 0xE8u;
  case ',':
    return 0xF4u;
  case '/':
    return 0xF3u;
  case ':':
    return 0x9Cu;
  case ';':
    return 0x9Du;
  case '(':
    return 0x9Au;
  case ')':
    return 0x9Bu;
  case '[':
    return 0x9Eu;
  case ']':
    return 0x9Fu;
  case '\'':
    return 0xE0u;
  case '#':
    return 0x54u;
  default:
    return 0u;
  }
}

YellowError yellow_encode_text(const char *in, uint8_t *out,
                               uint8_t *bad_index) YELLOW_BANKED {
  uint8_t i;
  uint8_t len = 0;

  while (in[len] != '\0') {
    len++;
    if (len > YELLOW_NAME_CHARS) {
      return YE_ERR_TEXT_TOO_LONG;
    }
  }
  for (i = 0; i < len; i++) {
    uint8_t b = encode_char(in[i]);
    if (b == 0u) {
      if (bad_index != 0) {
        *bad_index = i;
      }
      return YE_ERR_TEXT_UNENCODABLE;
    }
    out[i] = b;
  }
  for (; i < NAME_LEN; i++) {
    out[i] = TEXT_TERM;
  }
  return YE_OK;
}
