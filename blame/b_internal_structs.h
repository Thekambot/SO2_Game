#ifndef SO2_GAME_B_INTERNAL_STRUCTS_H
#define SO2_GAME_B_INTERNAL_STRUCTS_H

#include "b_internal_enums.h"
#include "b_internal_defines.h"

#include <stdint.h>

//typedef uint32_t PID;
typedef int shm_t;

typedef struct
{
    void *ptrA;
    void *ptrB;
} ArgData;

typedef struct
{
    char *map;
    int camp_x;
    int camp_y;
    int size_x;
    int size_y;

} MapData;

typedef struct
{
    unsigned int amount;
    int x;
    int y;

} LootInfo;

typedef struct
{
    pid_t this_PID;
    pid_t server_PID;

    EntityType entity_type;
    int8_t player_number;

    uint32_t spawn_x;
    uint32_t spawn_y;

    uint32_t cur_x;
    uint32_t cur_y;
    MapData fov;

    uint32_t death_counter;
    uint32_t coins_found_counter;
    uint32_t coins_bank_counter;

    enum {ACTION_NONE, ACTION_ACCEPTED, ACTION_REJECTED} ServerActions;
    KeyCode key;
    uint8_t action_cooldown;

    sem_t server_answer;
    sem_t client_answer;

} EntityInfo;

typedef struct
{
    shm_t fd;                 // JoinRequest
    void *memory_map;
    pthread_t thread;
    bool is_thread_running;

} SHMController;

typedef struct
{
    pid_t server_PID;

    pid_t player_pid;
    EntityType player_type;

    enum {JOIN_UNKNOWN, JOIN_ACCEPTED, JOIN_REJECTED} join_status;
    int player_number;

    sem_t server_open_request;
    sem_t server_new_request;
    sem_t server_checked_request;

} JoinRequest;

typedef struct
{
    pid_t this_PID;

    // EntityInfo *entities[MAX_PLAYERS];
    SHMController *shm_entities[MAX_ENTITIES];
    uint8_t player_count;
    uint8_t beasts_count;

    MapData *map;
    MapData *map_entities;

    LootInfo dropped_loots[50];
    uint32_t round_count;

    SHMController join_request;

} ServerInfo;

#endif //SO2_GAME_B_INTERNAL_STRUCTS_H
