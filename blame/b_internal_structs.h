#ifndef SO2_GAME_B_INTERNAL_STRUCTS_H
#define SO2_GAME_B_INTERNAL_STRUCTS_H

#include "b_internal_enums.h"
#include "b_internal_defines.h"

#include <stdint.h>

typedef uint32_t PID;

typedef struct
{
    PID this_PID;
    PID server_PID;

    EntityType entity_type;
    uint8_t player_number;

    uint32_t cur_x;
    uint32_t cur_y;

    uint32_t death_counter;
    uint32_t coins_found_counter;
    uint32_t coins_bank_counter;

} EntityInfo;

typedef struct
{
    char **map;
    uint8_t camp_x;
    uint8_t camp_y;
    uint8_t size_x;
    uint8_t size_y;

} MapData;

typedef struct
{
    PID this_PID;

    MapData map;

    EntityInfo entities[MAX_PLAYERS];
    uint8_t entities_size;

} ServerInfo;

#endif //SO2_GAME_B_INTERNAL_STRUCTS_H
