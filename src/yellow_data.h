/*
 * yellow_data.h - internal table declarations for yellow_data.c
 * (ROM bank 4). Not part of the public API; tables are read only by
 * the accessors in yellow_data.c.
 */

#ifndef YELLOW_DATA_H
#define YELLOW_DATA_H

#include <stdint.h>
#include "yellow.h"


typedef struct YellowMoveInfo {
    const char name[13];
    uint8_t power;
    uint8_t type_id;
    uint8_t accuracy; /* raw ROM byte, out of 255 */
    uint8_t base_pp;
} YellowMoveInfo;

extern const char yellow_species_names[YELLOW_SPECIES_COUNT + 1][13];
extern const YellowBaseStats yellow_base_stats_table[YELLOW_SPECIES_COUNT + 1];
extern const uint8_t yellow_index_to_dex_table[256];
extern const uint8_t yellow_dex_to_index_table[YELLOW_SPECIES_COUNT + 1];
extern const YellowMoveInfo yellow_moves_table[YELLOW_MOVE_COUNT + 1];
extern const char yellow_item_names[256][13];
extern const char yellow_type_names[27][10];

#endif /* YELLOW_DATA_H */
