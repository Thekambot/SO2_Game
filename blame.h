#ifndef SO2_GAME_BLAME_H
#define SO2_GAME_BLAME_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <err.h>
#include <pthread.h>
#include <stdbool.h>
#include <wchar.h>

#include "blame/b_internal_enums.h"
#include "blame/b_internal_structs.h"

//typedef void SeverInfo;
//typedef void EntityInfo;

ServerInfo  *server_init();
void        server_destroy(ServerInfo *);

int         server_thread_set(ServerInfo *server, SHMController *shm, void *(*func)(void *), void *arg);

void        *server_join_requests_thread_function(void *arg); // arg == ServerInfo
void        *server_timer_thread_function(void *arg); // arg == ServerInfo
void        *server_entity_thread_function(void *arg); // arg == ArgData = {ServerInfo, SHMController}

int         server_entity_add(ServerInfo *server, JoinRequest *join_info);
int         server_entity_delete(ServerInfo *server, int8_t player_number);
int         server_entity_shm_create(ServerInfo *server, JoinRequest *join_info, uint8_t player_number);
int         server_entity_key_action(ServerInfo *server, EntityInfo *entity, KeyCode key);
int         server_entity_respawn(ServerInfo *server, int8_t player_number);
int         server_entity_fov_update(ServerInfo *server, int8_t player_number);
int         server_entity_garbage_collector(ServerInfo *server);

int         server_game_entity_behavior(ServerInfo *server, EntityInfo *entity);
int         server_game_entity_player_behavior(ServerInfo *server, EntityInfo *entity); // NOT THREAD SAFE
int         server_game_entity_beast_behavior(ServerInfo *server, EntityInfo *entity);  // NOT THREAD SAFE

int         server_map_entity_position_update(ServerInfo *server, int8_t player_number);
int         server_map_object_delete(ServerInfo *server, uint32_t x, uint32_t y);
int         server_map_entity_delete(ServerInfo *server, int8_t player_number);
int         server_map_loot_create(ServerInfo *server, int8_t player_numberA, int8_t player_numberB);
int         server_map_loot_collect(ServerInfo *server, int8_t loot_number);

void        server_cool_down_subtract(ServerInfo *server);

EntityInfo  *entity_server_connection_start(EntityType type);
void        entity_server_connection_end(EntityInfo *entity);

int         entity_server_send_key(EntityInfo *entity, KeyCode key);
int         entity_server_status_check(EntityInfo *entity);

MapData     *map_load(char *);
void        map_destroy(MapData *);
int         map_random_empty_location(MapData *map, int *x, int *y);
void        map_random_generate_symbol(MapData *map, int symbol);
int         map_copy_fragment(MapData *src, int x, int y, MapData *dest);
int         map_copy_to_fov(MapData *src, int x, int y, char *dest);

#endif //SO2_GAME_BLAME_H
