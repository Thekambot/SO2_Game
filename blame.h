#ifndef SO2_GAME_BLAME_H
#define SO2_GAME_BLAME_H

#include "blame/b_internal_enums.h"
#include "blame/b_internal_structs.h"

//typedef void SeverInfo;
//typedef void EntityInfo;

// Gotowe funkcje do main

void        server_maintain(ServerInfo *);
void        entity_maintain(EntityInfo *);

// Właściwe API

ServerInfo  *server_init();
void        server_destroy(ServerInfo *);

EntityInfo  *entity_init(EntityType);
void        entity_destroy(EntityInfo *);

MapData     *map_load(char *);
void        map_destroy(MapData *);

#endif //SO2_GAME_BLAME_H
