#include "../blame.h"

ServerInfo *server_init()
{
    // ServerInfo Struct

    ServerInfo *server = (ServerInfo *) calloc(1, sizeof(ServerInfo));
    if (server == NULL)
        return NULL;

    server->this_PID = getpid();

    server->map = map_load(DEFAULT_MAP_PATH);
    server->map_entities = map_load(DEFAULT_MAP_PATH);

    server->player_count = 0;
    server->round_count = 0;

    server->join_request.fd = shm_open(SHM_JOIN_REQUEST, O_CREAT | O_RDWR, 0600);
    ftruncate(server->join_request.fd, sizeof(JoinRequest));
    server->join_request.memory_map = (JoinRequest *) mmap(NULL, sizeof(JoinRequest), PROT_READ | PROT_WRITE,
                                                           MAP_SHARED,
                                                           server->join_request.fd, 0);

    JoinRequest *request = (JoinRequest *) server->join_request.memory_map;
    sem_init(&request->server_open_request, 1, 0);
    sem_init(&request->server_new_request, 1, 0);
    sem_init(&request->server_checked_request, 1, 0);

    return server;
}

int server_thread_set(ServerInfo *server, SHMController *shm, void *(*func)(void *), void *arg)
{
    if (shm->is_thread_running == true)
        return 1;

    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);

    pthread_t thread;
    if (pthread_create(&thread, &thread_attr, func, arg) != 0)
    {
        pthread_attr_destroy(&thread_attr);
        return 1;
    }

    shm->thread = thread;
    shm->is_thread_running = true;

    return 0;
}

void *server_join_requests_thread_function(void *arg)
{
    ServerInfo *server = (ServerInfo *) arg;
    SHMController *controller = &server->join_request;
    JoinRequest *request = (JoinRequest *) controller->memory_map;

    while (controller->is_thread_running)
    {
        request->join_status = JOIN_UNKNOWN;

        sem_post(&request->server_open_request);
        sem_wait(&request->server_new_request);

        int test = server_entity_add(server, request);
        if (test)
        {
            request->join_status = JOIN_REJECTED;
            continue;
        }

        request->join_status = JOIN_ACCEPTED;
        sem_post(&request->server_checked_request);
    }

    controller->is_thread_running = false;

    return NULL;
}

int server_entity_create_shm(ServerInfo *server, JoinRequest *join_info, uint8_t player_number)
{
    if (player_number >= MAX_ENTITIES)
        return -1;

    if (server->shm_entities[player_number - 1] != NULL)
        return -1;

    server->shm_entities[player_number - 1] = (SHMController *) calloc(1, sizeof(SHMController));
    if (server->shm_entities[player_number - 1] == NULL)
        return -2;

    shm_t shm_player = -1;

    if (join_info->player_type != ENTITY_BEAST && player_number >= 1 && player_number <= MAX_PLAYERS)
    {
        if (player_number == 1) shm_player = shm_open(SHM_PLAYER1, O_CREAT | O_RDWR, 0600);
        else if (player_number == 2) shm_player = shm_open(SHM_PLAYER2, O_CREAT | O_RDWR, 0600);
        else if (player_number == 3) shm_player = shm_open(SHM_PLAYER3, O_CREAT | O_RDWR, 0600);
        else if (player_number == 4) shm_player = shm_open(SHM_PLAYER4, O_CREAT | O_RDWR, 0600);
    }
    else if (join_info->player_type == ENTITY_BEAST && player_number > MAX_PLAYERS)
    {
        if (player_number == MAX_PLAYERS + 1) shm_player = shm_open(SHM_BEAST1, O_CREAT | O_RDWR, 0600);
        else if (player_number == MAX_PLAYERS + 2) shm_player = shm_open(SHM_BEAST2, O_CREAT | O_RDWR, 0600);
    }
    if (shm_player == -1)
    {
        free(server->shm_entities[player_number - 1]);
        return -3;
    }

    ftruncate(shm_player, sizeof(EntityInfo));
    EntityInfo *player = (EntityInfo *) mmap(NULL, sizeof(EntityInfo), PROT_READ | PROT_WRITE, MAP_SHARED, shm_player, 0);

    player->player_number = player_number;

    player->this_PID = join_info->player_pid;
    player->entity_type = join_info->player_type;
    player->server_PID = server->this_PID;

    player->coins_bank_counter = 0;
    player->coins_found_counter = 0;
    player->death_counter = 0;
    map_random_empty_location(server->map, &player->cur_x, &player->cur_y);
    player->spawn_x = player->cur_x;
    player->spawn_y = player->cur_y;

    player->key = K_NONE;
    player->ServerActions = ACTION_NONE;
    player->action_cooldown = 1;
    
    sem_init(&player->server_answer, 1, 0);
    sem_init(&player->client_answer, 1, 0);

    server->shm_entities[player_number - 1]->fd = shm_player;
    server->shm_entities[player_number - 1]->memory_map = player;

    player->fov.size_x = PLAYER_FOV_X;
    player->fov.size_y = PLAYER_FOV_Y;
    if (map_copy_fragment(server->map_entities, player->cur_x, player->cur_y, &player->fov))
        server_entity_delete(server, player->player_number);

}

int server_entity_add(ServerInfo *server, JoinRequest *join_info)
{
    if (server->beasts_count + server->player_count >= MAX_ENTITIES)
        return -1;

    if (server->player_count > MAX_PLAYERS && (join_info->player_type == ENTITY_HUMAN || join_info->player_type == ENTITY_BOT))
        return -1;
    if (server->beasts_count > MAX_BEASTS && join_info->player_type == ENTITY_BEAST)
        return -1;

    uint8_t player_index = (join_info->player_type == ENTITY_BEAST) ? MAX_PLAYERS : 0;
    for (; player_index < MAX_PLAYERS; ++player_index)
    {
        if (server->shm_entities[player_index] == NULL)
            break;
    }

    server_entity_create_shm(server, join_info, player_index + 1);

    ArgData arg = {server, server->shm_entities[player_index]};
    server_thread_set(server, server->shm_entities[player_index], server_entity_thread_function, &arg);

    if (join_info->player_type == ENTITY_BEAST)
        server->beasts_count++;
    else
        server->player_count++;

    return 1;
}

void *server_entity_thread_function(void *arg)
{
    ArgData *argPtr = (ArgData *) arg;

    ServerInfo *server = argPtr->ptrA;
    SHMController *controller = argPtr->ptrB;
    EntityInfo *entity = (EntityInfo *) controller->memory_map;

    while (controller->is_thread_running && kill(entity->this_PID, 0) != -1)
    {
        sem_wait(&entity->client_answer);

        int test = server_entity_key_action(server, entity, entity->key);
        if (test)
            entity->ServerActions = ACTION_REJECTED;
        else
            entity->ServerActions = ACTION_ACCEPTED;

        entity->key = K_NONE;

        sem_post(&entity->server_answer);
    }

    controller->is_thread_running = false;
    entity->entity_type = ENTITY_TO_DESTROY;

    return NULL;
}

int server_entity_key_action(ServerInfo *server, EntityInfo *entity, KeyCode key)
{
    if (key == K_QUIT)
    {
        entity->entity_type = ENTITY_TO_DESTROY;
        return -1;
    }

    if (entity->action_cooldown > 0)
        return 1;

    uint32_t x = entity->cur_x;
    uint32_t y = entity->cur_y;

    if (key == K_DOWN) entity->cur_y--;
    else if (key == K_UP) entity->cur_y++;
    else if (key == K_RIGHT) entity->cur_x++;
    else if (key == K_LEFT) entity->cur_x--;

    int test = server_game_entity_behavior(server, entity);
    if (test)
    {
        entity->cur_x = x;
        entity->cur_y = y;
        return 1;
    }

    entity->action_cooldown++;

    return 0;
}

int server_entity_respawn(ServerInfo *server, int8_t player_number)
{
    EntityInfo *entity = (EntityInfo *)server->shm_entities[player_number - 1]->memory_map;

    entity->cur_x = entity->spawn_x;
    entity->cur_y = entity->spawn_y;

    return 0;
}

int server_game_entity_behavior(ServerInfo *server, EntityInfo *entity)
{
    uint32_t x = entity->cur_x;
    uint32_t y = entity->cur_y;

    char object = server->map->map[y * server->map->size_x + x];
    char object_entity = server->map_entities->map[y * server->map->size_x + x];

    if (object == ':') return 1;

    if (object == '#') entity->action_cooldown++;

    if (object == 'A')
    {
        entity->coins_bank_counter += entity->coins_found_counter;
        entity->coins_found_counter = 0;
    }
    else if (object == 'c' || object == 't' || object == 'T' || object < 0)
    {
        if (object == 'c') entity->coins_found_counter += 1;
        else if (object == 't') entity->coins_found_counter += 5;
        else if (object == 'T') entity->coins_found_counter += 50;
        else if (object < 0) entity->coins_found_counter += server_map_loot_collect(server, -object);

        server_map_object_delete(server, entity->cur_x, entity->cur_y);
    }
    else if (object_entity >= 1 && object_entity <= MAX_PLAYERS)
    {
        server_map_entity_delete(server, object);
        server_map_entity_delete(server, entity->player_number);

        server_map_loot_create(server, entity->player_number, object);

        server_entity_respawn(server, entity->player_number);
        server_entity_respawn(server, object);

        server_map_entity_position_update(server, object);
    }
    else if (object_entity >= MAX_PLAYERS)
    {
        server_map_entity_delete(server, entity->player_number);

        server_map_loot_create(server, entity->player_number, 0);

        server_entity_respawn(server, entity->player_number);
    }

    server_map_entity_position_update(server, entity->player_number);

    return 0;
}

int server_map_entity_position_update(ServerInfo *server, int8_t player_number)
{
    char *map = server->map_entities->map;
    EntityInfo *entity = (EntityInfo *)server->shm_entities[player_number - 1]->memory_map;

    map[entity->cur_y * server->map_entities->size_x + entity->cur_x] = (char) player_number;

    return 0;
}

int server_map_object_delete(ServerInfo *server, uint32_t x, uint32_t y)
{
    server->map->map[y * server->map->size_y + x] = ' ';

    return 0;
}

int server_map_entity_delete(ServerInfo *server, int8_t player_number)
{
    char *map = server->map_entities->map;
    EntityInfo *entity = (EntityInfo *)server->shm_entities[player_number - 1]->memory_map;

    entity->action_cooldown = 1;
    entity->death_counter++;
    map[entity->cur_y * server->map_entities->size_x + entity->cur_x] = '\0';

    return 0;
}

int server_map_loot_create(ServerInfo *server, int8_t player_numberA, int8_t player_numberB)
{
    char *map = server->map->map;
    EntityInfo *entityA = (EntityInfo *)server->shm_entities[player_numberA - 1]->memory_map;
    EntityInfo *entityB = (EntityInfo *)server->shm_entities[player_numberB - 1]->memory_map;

    EntityInfo empty;
    empty.coins_found_counter = 0;
    if (player_numberB == 0)
        entityB = &empty;

    int loot_index = 1;
    for (; loot_index < 50; ++loot_index)
        if (server->dropped_loots[loot_index].amount == 0)
            break;

    server->dropped_loots[loot_index].amount = entityA->coins_found_counter + entityB->coins_found_counter;
    entityA->coins_found_counter = 0;
    entityB->coins_found_counter = 0;

    server->dropped_loots[loot_index].x = entityA->cur_x;
    server->dropped_loots[loot_index].y = entityA->cur_y;

    map[entityA->cur_y * server->map_entities->size_x + entityA->cur_x] = (char)(-loot_index);

    return 0;
}

int server_map_loot_collect(ServerInfo *server, int8_t loot_number)
{
    char *map = server->map->map;
    unsigned int amount = server->dropped_loots[loot_number].amount;

    map[server->dropped_loots[loot_number].y * server->map->size_x + server->dropped_loots[loot_number].x] = ' ';

    server->dropped_loots[loot_number].amount = 0;

    return (int) amount;
}

int server_entity_delete(ServerInfo *server, int8_t player_number)
{
    SHMController *toDelete = server->shm_entities[player_number - 1];
    if (toDelete == NULL)
        return -1;

    pthread_cancel(toDelete->thread);
    toDelete->is_thread_running = false;

    EntityInfo *entity = (EntityInfo *)toDelete->memory_map;
    free(entity->fov.map);
    sem_destroy(&entity->server_answer);
    sem_destroy(&entity->client_answer);
    munmap(entity, sizeof(EntityInfo *));
    close(toDelete->fd);

    // HARD CODED; MUST CHANGE
    if (player_number == 1) shm_unlink(SHM_PLAYER1);
    if (player_number == 2) shm_unlink(SHM_PLAYER2);
    if (player_number == 3) shm_unlink(SHM_PLAYER3);
    if (player_number == 4) shm_unlink(SHM_PLAYER4);
    if (player_number == 5) shm_unlink(SHM_BEAST1);
    if (player_number == 6) shm_unlink(SHM_BEAST2);

    return 0;
}

void server_destroy(ServerInfo *ptr)
{
    for (int i = 0; i < MAX_ENTITIES; ++i)
    {
        if (ptr->shm_entities[i] != NULL)
        {
            server_entity_delete(ptr, ((EntityInfo *)ptr->shm_entities[i]->memory_map)->player_number);
        }
    }

    map_destroy(ptr->map);
    map_destroy(ptr->map_entities);

    if (ptr->join_request.is_thread_running)
        pthread_cancel(ptr->join_request.thread);
    JoinRequest *request = (JoinRequest *) ptr->join_request.memory_map;
    sem_destroy(&request->server_open_request);
    sem_destroy(&request->server_new_request);
    sem_destroy(&request->server_checked_request);
    munmap(ptr->join_request.memory_map, sizeof(JoinRequest));
    close(ptr->join_request.fd);
    shm_unlink(SHM_JOIN_REQUEST);

    free(ptr);
}
