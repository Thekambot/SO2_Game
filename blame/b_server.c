#include "../blame.h"

ServerInfo *server_init()
{
    // ServerInfo Struct

    ServerInfo *server = (ServerInfo *) calloc(1, sizeof(ServerInfo));
    if (server == NULL)
        return NULL;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&server->mutex, &attr);

    pthread_mutex_lock(&server->mutex);

    server->this_PID = getpid();

    server->map = map_load(DEFAULT_MAP_PATH);
    server->map_entities = map_load(DEFAULT_MAP_PATH);
    memset(server->map_entities->map, '\0', server->map_entities->size_x * server->map_entities->size_y);

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

    server_thread_set(server, &server->join_request, server_join_requests_thread_function, server);
    server_thread_set(server, &server->timer, server_timer_thread_function, server);

    pthread_mutex_unlock(&server->mutex);

    return server;
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

    if (ptr->timer.is_thread_running)
    {
        ptr->timer.is_thread_running = false;
        //pthread_cancel(ptr->timer.thread);
        pthread_join(ptr->timer.thread, NULL);
    }

    pthread_mutex_lock(&ptr->mutex);

    JoinRequest *request = (JoinRequest *) ptr->join_request.memory_map;

    if (ptr->join_request.is_thread_running)
    {
        ptr->join_request.is_thread_running = false;
        sem_post(&request->server_new_request);
        pthread_join(ptr->join_request.thread, NULL);
    }

    sem_destroy(&request->server_open_request);
    sem_destroy(&request->server_new_request);
    sem_destroy(&request->server_checked_request);
    munmap(ptr->join_request.memory_map, sizeof(JoinRequest));
    close(ptr->join_request.fd);
    shm_unlink(SHM_JOIN_REQUEST);

    pthread_mutex_unlock(&ptr->mutex);

    pthread_mutex_destroy(&ptr->mutex);

    free(ptr);
}


int server_thread_set(ServerInfo *server, SHMController *shm, void *(*func)(void *), void *arg)
{
    if (shm->is_thread_running == true)
        return 1;

    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);

    pthread_t thread;
    shm->is_thread_running = true;

    if (pthread_create(&thread, &thread_attr, func, arg) != 0)
    {
        pthread_attr_destroy(&thread_attr);
        return 1;
    }

    shm->thread = thread;

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

        if (controller->is_thread_running == false)
            break;

        pthread_mutex_lock(&server->mutex);

        int test = server_entity_add(server, request);
        if (test)
            request->join_status = JOIN_REJECTED;
        else
            request->join_status = JOIN_ACCEPTED;

        sem_post(&request->server_checked_request);
        sem_wait(&request->server_new_request);

        pthread_mutex_unlock(&server->mutex);
    }

    controller->is_thread_running = false;

    return NULL;
}

void *server_timer_thread_function(void *arg)
{
    ServerInfo *server = (ServerInfo *) arg;
    SHMController *controller = &server->timer;

    clock_t begin;
    bool full_round = true;

    while (controller->is_thread_running)
    {
        if (full_round == true)
        {
            begin = clock();
            full_round = false;
        }

        server_entity_garbage_collector(server);

        double time_spent = (double)(clock() - begin) / CLOCKS_PER_SEC;
        if (time_spent > 1.0)
        {
            for (int i = 1; i < MAX_ENTITIES; ++i)
                if (server->shm_entities[i - 1] != NULL)
                    server_entity_fov_update(server, i);

            server->round_count++;
            server_cool_down_subtract(server);
            full_round = true;
        }
    }

    controller->is_thread_running = false;

    return NULL;
}

int server_entity_shm_create(ServerInfo *server, JoinRequest *join_info, uint8_t player_number)
{
    pthread_mutex_lock(&server->mutex);

    if (player_number >= MAX_ENTITIES)
    {
        pthread_mutex_unlock(&server->mutex);
        return -1;
    }

    if (server->shm_entities[player_number - 1] != NULL)
    {
        pthread_mutex_unlock(&server->mutex);
        return -1;
    }

    server->shm_entities[player_number - 1] = (SHMController *) calloc(1, sizeof(SHMController));
    if (server->shm_entities[player_number - 1] == NULL)
    {
        pthread_mutex_unlock(&server->mutex);
        return -2;
    }

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
        server->shm_entities[player_number - 1] = NULL;
        pthread_mutex_unlock(&server->mutex);
        return -3;
    }

    ftruncate(shm_player, sizeof(EntityInfo));
    EntityInfo *player = (EntityInfo *) mmap(NULL, sizeof(EntityInfo), PROT_READ | PROT_WRITE, MAP_SHARED, shm_player, 0);

    player->player_number = player_number;

    player->this_PID = join_info->player_pid;
    player->entity_type = join_info->player_type;
    player->server_PID = server->this_PID;
    player->new_entity = true;

    player->coins_bank_counter = 0;
    player->coins_found_counter = 0;
    player->death_counter = 0;

    do { map_random_empty_location(server->map, &player->cur_x, &player->cur_y); }
    while (server->map_entities->map[player->cur_y * server->map_entities->size_x + player->cur_x] != '\0');
    player->spawn_x = player->cur_x;
    player->spawn_y = player->cur_y;

    player->key = K_NONE;
    player->action_status = ACTION_NONE;
    player->action_cooldown = 1;
    
    sem_init(&player->server_answer, 1, 0);
    sem_init(&player->client_answer, 1, 0);

    server->shm_entities[player_number - 1]->fd = shm_player;
    server->shm_entities[player_number - 1]->memory_map = player;

    server_entity_fov_update(server, player_number);

    pthread_mutex_unlock(&server->mutex);

    return 0;
}

int server_entity_add(ServerInfo *server, JoinRequest *join_info)
{
    if (server->beasts_count + server->player_count >= MAX_ENTITIES)
        return -1;

    if (server->player_count >= MAX_PLAYERS && (join_info->player_type == ENTITY_HUMAN || join_info->player_type == ENTITY_BOT))
        return -1;
    if (server->beasts_count >= MAX_BEASTS && join_info->player_type == ENTITY_BEAST)
        return -1;

    pthread_mutex_lock(&server->mutex);

    uint8_t player_index = (join_info->player_type == ENTITY_BEAST) ? MAX_PLAYERS : 0;
    for (; player_index < MAX_PLAYERS; ++player_index)
    {
        if (server->shm_entities[player_index] == NULL)
            break;
    }

    server_entity_shm_create(server, join_info, player_index + 1);

    if (join_info->player_type == ENTITY_BEAST)
        server->beasts_count++;
    else
        server->player_count++;

    server_map_entity_position_update(server, player_index + 1);

    server_thread_set(server, server->shm_entities[player_index], server_entity_thread_function, server);

    join_info->server_PID = server->this_PID;
    join_info->player_number = player_index + 1;

    pthread_mutex_unlock(&server->mutex);

    return 0;
}

void *server_entity_thread_function(void *arg)
{
    ServerInfo *server = arg;
    SHMController *controller;
    EntityInfo *entity;

    int debug_test = 0;

    for (int i = 0; i < MAX_ENTITIES; ++i)
    {
        if (server->shm_entities[i] == NULL) continue;

        controller = server->shm_entities[i];
        entity = (EntityInfo *)controller->memory_map;

        if (entity->new_entity)
        {
            entity->new_entity = false;
            debug_test = 1;
            break;
        }
    }

    assert(debug_test == 1 || !"Nie ma new_entity == 0");

    while (controller->is_thread_running && kill(entity->this_PID, 0) != -1)
    {
        sem_wait(&entity->client_answer);
        if (controller->is_thread_running == false)
            break;

        pthread_mutex_lock(&server->mutex);

        int test = server_entity_key_action(server, entity, entity->key);
        if (test)
            entity->action_status = ACTION_REJECTED;
        else
            entity->action_status = ACTION_ACCEPTED;

        entity->key = K_NONE;

        sem_post(&entity->server_answer);

        pthread_mutex_unlock(&server->mutex);
    }

    controller->is_thread_running = false;
    entity->entity_type = ENTITY_TO_DESTROY;

    return NULL;
}

int server_entity_key_action(ServerInfo *server, EntityInfo *entity, KeyCode key)
{
    if (key == K_NONE)
        return 1;

    if (key == K_QUIT)
    {
        entity->entity_type = ENTITY_TO_DESTROY;
        return -1;
    }

    if (entity->action_cooldown > 0)
        return 1;

    uint32_t x = entity->cur_x;
    uint32_t y = entity->cur_y;

    server_map_entity_delete(server, entity->player_number);

    if (key == K_DOWN) entity->cur_y++;
    else if (key == K_UP) entity->cur_y--;
    else if (key == K_RIGHT) entity->cur_x++;
    else if (key == K_LEFT) entity->cur_x--;

    int test = server_game_entity_behavior(server, entity);
    if (test)
    {
        // assert(!"Oho, zły ruch kolego haha\n");
        entity->cur_x = x;
        entity->cur_y = y;
        server_map_entity_position_update(server, entity->player_number);

        return 1;
    }

    entity->action_cooldown++;

    return 0;
}

int server_entity_respawn(ServerInfo *server, int8_t player_number)
{
    if (server->shm_entities[player_number - 1] == NULL)
        return 1;

    pthread_mutex_lock(&server->mutex);

    EntityInfo *entity = (EntityInfo *)server->shm_entities[player_number - 1]->memory_map;

    entity->death_counter++;
    entity->action_cooldown = 1;
    entity->cur_x = entity->spawn_x;
    entity->cur_y = entity->spawn_y;

    pthread_mutex_unlock(&server->mutex);

    return 0;
}

int server_entity_fov_update(ServerInfo *server, int8_t player_number)
{
    if (server->shm_entities[player_number - 1] == NULL)
        return 1;

    pthread_mutex_lock(&server->mutex);

    EntityInfo *entity = (EntityInfo *)server->shm_entities[player_number - 1]->memory_map;

    int x = (int) entity->cur_x - (PLAYER_FOV_X / 2);
    int y = (int) entity->cur_y - (PLAYER_FOV_Y / 2);

    map_copy_to_fov(server->map, x, y, entity->fov);
    map_copy_to_fov(server->map_entities, x, y, entity->fov);

    pthread_mutex_unlock(&server->mutex);

    return 0;
}

int server_entity_garbage_collector(ServerInfo *server)
{
    pthread_mutex_lock(&server->mutex);

    for (int i = 1; i < MAX_ENTITIES; ++i)
    {
        if (server->shm_entities[i - 1] == NULL)
            continue;

        EntityInfo *entity = (EntityInfo *) server->shm_entities[i - 1]->memory_map;

        if (entity->entity_type == ENTITY_TO_DESTROY || kill(entity->this_PID, 0) == -1)
            server_entity_delete(server, i);
    }

    pthread_mutex_unlock(&server->mutex);

    return 0;
}

int server_game_entity_behavior(ServerInfo *server, EntityInfo *entity)
{
    pthread_mutex_lock(&server->mutex);

    uint32_t x = entity->cur_x;
    uint32_t y = entity->cur_y;

    char object = server->map->map[y * server->map->size_x + x];
    char object_entity = server->map_entities->map[y * server->map->size_x + x];

    if (object == ':')
    {
        pthread_mutex_unlock(&server->mutex);
        return 1;
    }

    if (object == '#') entity->action_cooldown++;

    if ((object != ' ' && object != '#') || object_entity != '\0')
    {
        int test = 1;
        if (entity->entity_type == ENTITY_HUMAN || entity->entity_type == ENTITY_BOT)
            test = server_game_entity_player_behavior(server, entity);
        else if (entity->entity_type == ENTITY_BEAST)
            test = server_game_entity_beast_behavior(server, entity);

        if (test)
        {
            pthread_mutex_unlock(&server->mutex);
            return test;
        }
    }

    server_map_entity_position_update(server, entity->player_number);
    server_entity_fov_update(server, entity->player_number);

    pthread_mutex_unlock(&server->mutex);

    return 0;
}

int server_game_entity_player_behavior(ServerInfo *server, EntityInfo *entity)
{
    uint32_t x = entity->cur_x;
    uint32_t y = entity->cur_y;

    char object = server->map->map[y * server->map->size_x + x];
    char object_entity = server->map_entities->map[y * server->map->size_x + x];

    if (object == 'A')
    {
        entity->coins_bank_counter += entity->coins_found_counter;
        entity->coins_found_counter = 0;

        return 0;
    }
    if (object == 'c' || object == 't' || object == 'T' || object < 0)
    {
        if (object == 'c') entity->coins_found_counter += 1;
        else if (object == 't') entity->coins_found_counter += 5;
        else if (object == 'T') entity->coins_found_counter += 50;
        else if (object < 0) entity->coins_found_counter += server_map_loot_collect(server, -object);

        server_map_object_delete(server, x, y);

        return 0;
    }
    if (object_entity >= 1 && object_entity <= MAX_PLAYERS)
    {
        server_map_entity_delete(server, object_entity);
        server_map_entity_delete(server, entity->player_number);

        server_map_loot_create(server, entity->player_number, object_entity);

        server_entity_respawn(server, entity->player_number);
        server_entity_respawn(server, object_entity);

        server_map_entity_position_update(server, object_entity);
        server_entity_fov_update(server, object_entity);

        return 0;
    }
    if (object_entity >= MAX_PLAYERS)
    {
        server_map_entity_delete(server, entity->player_number);

        server_map_loot_create(server, entity->player_number, 0);

        server_entity_respawn(server, entity->player_number);

        return 0;
    }

    return 1;
}

int server_game_entity_beast_behavior(ServerInfo *server, EntityInfo *entity)
{
    uint32_t x = entity->cur_x;
    uint32_t y = entity->cur_y;

    char object = server->map->map[y * server->map->size_x + x];
    char object_entity = server->map_entities->map[y * server->map->size_x + x];

    if (object_entity > MAX_PLAYERS || object == 'A')
        return 1;

    if (object_entity > 0 && object_entity <= MAX_PLAYERS)
    {
        server_map_entity_delete(server, object_entity);

        server_map_loot_create(server, object_entity, 0);

        server_entity_respawn(server, object_entity);
        server_entity_fov_update(server, object_entity);

        return 0;
    }

    return 1;
}

int server_map_entity_position_update(ServerInfo *server, int8_t player_number)
{
    if (server->shm_entities[player_number - 1] == NULL)
        return 1;

    pthread_mutex_lock(&server->mutex);

    char *map = server->map_entities->map;
    EntityInfo *entity = (EntityInfo *)server->shm_entities[player_number - 1]->memory_map;

    map[entity->cur_y * server->map_entities->size_x + entity->cur_x] = (char) player_number;

    pthread_mutex_unlock(&server->mutex);

    return 0;
}

int server_map_object_delete(ServerInfo *server, uint32_t x, uint32_t y)
{
    pthread_mutex_lock(&server->mutex);

    server->map->map[y * server->map->size_x + x] = ' ';

    pthread_mutex_unlock(&server->mutex);

    return 0;
}

int server_map_entity_delete(ServerInfo *server, int8_t player_number)
{
    if (server->shm_entities[player_number - 1] == NULL)
        return 1;

    pthread_mutex_lock(&server->mutex);

    char *map = server->map_entities->map;
    EntityInfo *entity = (EntityInfo *)server->shm_entities[player_number - 1]->memory_map;

    // entity->action_cooldown = 1;
    map[entity->cur_y * server->map_entities->size_x + entity->cur_x] = '\0';

    pthread_mutex_unlock(&server->mutex);

    return 0;
}

int server_map_loot_create(ServerInfo *server, int8_t player_numberA, int8_t player_numberB)
{
    pthread_mutex_lock(&server->mutex);

    char *map = server->map->map;
    EntityInfo *entityA;
    EntityInfo *entityB;

    EntityInfo empty;
    empty.coins_found_counter = 0;

    if (player_numberA == 0 || player_numberA >= MAX_PLAYERS)
        entityA = &empty;
    else
        entityA = (EntityInfo *)server->shm_entities[player_numberA - 1]->memory_map;

    if (player_numberB == 0 || player_numberB >= MAX_PLAYERS)
        entityB = &empty;
    else
        entityB = (EntityInfo *)server->shm_entities[player_numberB - 1]->memory_map;

    if (&entityA == &entityB)
        return 1;

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

    pthread_mutex_unlock(&server->mutex);

    return 0;
}

int server_map_loot_collect(ServerInfo *server, int8_t loot_number)
{
    pthread_mutex_lock(&server->mutex);

    char *map = server->map->map;
    unsigned int amount = server->dropped_loots[loot_number].amount;

    map[server->dropped_loots[loot_number].y * server->map->size_x + server->dropped_loots[loot_number].x] = ' ';

    server->dropped_loots[loot_number].amount = 0;

    pthread_mutex_unlock(&server->mutex);

    return (int) amount;
}

void server_cool_down_subtract(ServerInfo *server)
{
    for (int i = 0; i < MAX_ENTITIES; ++i)
    {
        if (server->shm_entities[i] == NULL)
            continue;

        EntityInfo *entity = (EntityInfo *) server->shm_entities[i]->memory_map;

        if (entity->action_cooldown > 0)
            entity->action_cooldown--;

        entity->round = server->round_count;
    }
}

int server_entity_delete(ServerInfo *server, int8_t player_number)
{
    SHMController *toDelete = server->shm_entities[player_number - 1];
    if (toDelete == NULL)
        return -1;

    EntityInfo *entity = (EntityInfo *)toDelete->memory_map;

    pthread_mutex_lock(&server->mutex);

    if (toDelete->is_thread_running == true)
    {
        toDelete->is_thread_running = false;
        sem_post(&entity->client_answer);
        pthread_join(toDelete->thread, NULL);
    }

    if (entity->coins_found_counter > 0) server_map_loot_create(server, player_number, 0);
    server_map_entity_delete(server, player_number);

    sem_destroy(&entity->server_answer);
    sem_destroy(&entity->client_answer);

    // memset(entity, 0, sizeof(EntityInfo));

    munmap(entity, sizeof(EntityInfo *));
    close(toDelete->fd);

    // HARD CODED; MUST CHANGE
    if (player_number == 1) shm_unlink(SHM_PLAYER1);
    if (player_number == 2) shm_unlink(SHM_PLAYER2);
    if (player_number == 3) shm_unlink(SHM_PLAYER3);
    if (player_number == 4) shm_unlink(SHM_PLAYER4);
    if (player_number == 5) shm_unlink(SHM_BEAST1);
    if (player_number == 6) shm_unlink(SHM_BEAST2);

    server->shm_entities[player_number - 1] = NULL;

    pthread_mutex_unlock(&server->mutex);

    return 0;
}
