#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "../blame.h"

EntityInfo *entity_server_connection_start(EntityType type)
{
    shm_t join_fd = shm_open(SHM_JOIN_REQUEST, O_RDWR, 0600);
    if (join_fd == -1)
        return NULL;

    ftruncate(join_fd, sizeof(JoinRequest));
    JoinRequest *join_shm = (JoinRequest *) mmap(NULL, sizeof(JoinRequest), PROT_WRITE | PROT_READ, MAP_SHARED, join_fd,
                                                 0);

    sem_wait(&join_shm->server_open_request);

    join_shm->player_type = type;
    join_shm->player_pid = getpid();

    sem_post(&join_shm->server_new_request);
    sem_wait(&join_shm->server_checked_request);

    bool join_status = true;
    if (join_shm->join_status == JOIN_REJECTED)
        join_status = false;

    pid_t server_pid = join_shm->server_PID;
    int player_number = join_shm->player_number;

    sem_post(&join_shm->server_new_request);

    munmap(join_shm, sizeof(JoinRequest));
    close(join_fd);

    if (join_status == false)
        return (void *)-1;
    if (kill(server_pid, 0) == -1)
        return (void *)-2;

    shm_t entity_fd = -1;
    if (type != ENTITY_BEAST && player_number >= 1 && player_number <= MAX_PLAYERS)
    {
        if (player_number == 1) entity_fd = shm_open(SHM_PLAYER1, O_CREAT | O_RDWR, 0600);
        else if (player_number == 2) entity_fd = shm_open(SHM_PLAYER2, O_CREAT | O_RDWR, 0600);
        else if (player_number == 3) entity_fd = shm_open(SHM_PLAYER3, O_CREAT | O_RDWR, 0600);
        else if (player_number == 4) entity_fd = shm_open(SHM_PLAYER4, O_CREAT | O_RDWR, 0600);
    }
    else if (type == ENTITY_BEAST && player_number > MAX_PLAYERS)
    {
        if (player_number == MAX_PLAYERS + 1) entity_fd = shm_open(SHM_BEAST1, O_CREAT | O_RDWR, 0600);
        else if (player_number == MAX_PLAYERS + 2) entity_fd = shm_open(SHM_BEAST2, O_CREAT | O_RDWR, 0600);
    }
    if (entity_fd == -1)
        return (void *)-3;

    ftruncate(join_fd, sizeof(EntityInfo));
    EntityInfo *entity = (EntityInfo *) mmap(NULL, sizeof(EntityInfo), PROT_WRITE | PROT_READ, MAP_SHARED, entity_fd,
                                              0);

    entity->fd = entity_fd;

    return entity;
}

void entity_server_connection_end(EntityInfo *entity)
{
    shm_t fd = entity->fd;

    munmap(entity, sizeof(EntityInfo));
    close(fd);
}

void *entity_beast_thread_function(void *arg)
{
    assert(arg != NULL);

    SHMController *controller = (SHMController *) arg;

    EntityInfo *entity = entity_server_connection_start(ENTITY_BEAST);
    if (entity == NULL)
    {
        assert(!"Connection nie dziala? (entity_beast_thread_function)");
        controller->is_thread_running = false;
        return NULL;
    }

    controller->memory_map = entity;

    int x = -1, y = -1;

    while (entity->entity_type == ENTITY_BEAST && controller->is_thread_running)
    {
        sem_wait(&entity->update);

        if (entity->entity_type != ENTITY_BEAST || controller->is_thread_running == false)
            break;

        int test = entity_beast_player_search(entity, &x, &y);

        if (test == 0)
            entity_beast_direction_move(entity, x, y);
    }

    entity->entity_type = ENTITY_TO_DESTROY;
    controller->is_thread_running = false;

    return NULL;
}

SHMController *entity_beast_add()
{
    SHMController *beast = calloc(1, sizeof(SHMController));
    if (beast == NULL)
        return NULL;

    pthread_t thread;
    beast->is_thread_running = true;

    pthread_create(&thread, NULL, entity_beast_thread_function, beast);

    beast->thread = thread;

    return beast;
}

void entity_beast_delete(SHMController *controller)
{
    if (controller == NULL)
        return;

    EntityInfo *beast = (EntityInfo *) controller->memory_map;

    controller->is_thread_running = false;
    sem_post(&beast->update);
    beast->entity_type = ENTITY_TO_DESTROY;
    pthread_join(controller->thread, NULL);

    entity_server_connection_end(beast);

    free(controller);
}

int entity_beast_player_search(EntityInfo *entity, int *x, int *y)
{
    char *map = entity->fov;

    int size_x = PLAYER_FOV_X;

    int cur_x = 2;
    int cur_y = 2;

    // up
    for (int i = 1; i <= 2; ++i)
    {
        char temp = map[(cur_y - i) * size_x + cur_x];

        if (temp == ':') break;
        if (temp > 0 && temp <= MAX_PLAYERS)
        {
            *x = cur_x;
            *y = cur_y - i;
            return 0;
        }
    }

    // down
    for (int i = 1; i <= 2; ++i)
    {
        char temp = map[(cur_y + i) * size_x + cur_x];

        if (temp == ':') break;
        if (temp > 0 && temp <= MAX_PLAYERS)
        {
            *x = cur_x;
            *y = cur_y + i;
            return 0;
        }
    }

    // right
    for (int i = 1; i <= 2; ++i)
    {
        char temp = map[cur_y * size_x + cur_x + i];

        if (temp == ':') break;
        if (temp > 0 && temp <= MAX_PLAYERS)
        {
            *x = cur_x + i;
            *y = cur_y;
            return 0;
        }
    }

    // left
    for (int i = 1; i <= 2; ++i)
    {
        char temp = map[cur_y * size_x + cur_x - i];

        if (temp == ':') break;
        if (temp > 0 && temp <= MAX_PLAYERS)
        {
            *x = cur_x - i;
            *y = cur_y;
            return 0;
        }
    }

    //left-up
    for (int i = 1; i <= 2; ++i)
    {
        char temp = map[(cur_y - i) * size_x + cur_x - i];

        if (temp == ':') break;
        if (temp > 0 && temp <= MAX_PLAYERS)
        {
            *x = cur_x - i;
            *y = cur_y - i;
            return 0;
        }
    }

    //left-down
    for (int i = 1; i <= 2; ++i)
    {
        char temp = map[(cur_y + i) * size_x + cur_x - i];

        if (temp == ':') break;
        if (temp > 0 && temp <= MAX_PLAYERS)
        {
            *x = cur_x - i;
            *y = cur_y + i;
            return 0;
        }
    }

    //right-up
    for (int i = 1; i <= 2; ++i)
    {
        char temp = map[(cur_y - i) * size_x + cur_x + i];

        if (temp == ':') break;
        if (temp > 0 && temp <= MAX_PLAYERS)
        {
            *x = cur_x + i;
            *y = cur_y - i;
            return 0;
        }
    }

    //right-down
    for (int i = 1; i <= 2; ++i)
    {
        char temp = map[(cur_y + i) * size_x + cur_x + i];

        if (temp == ':') break;
        if (temp > 0 && temp <= MAX_PLAYERS)
        {
            *x = cur_x + i;
            *y = cur_y + i;
            return 0;
        }
    }

    return 1;
}

int entity_beast_direction_move(EntityInfo *entity, int x, int y)
{
    int cur_x = 2;
    int cur_y = 2;

    if (x < cur_x && y < cur_y)
    {
        if (entity_server_send_key(entity, K_LEFT))
            return entity_server_send_key(entity, K_UP);
        else
            return 0;
    }
    if (x < cur_x && y > cur_y)
    {
        if (entity_server_send_key(entity, K_LEFT))
            return entity_server_send_key(entity, K_DOWN);
        else
            return 0;
    }
    if (x > cur_x && y < cur_y)
    {
        if (entity_server_send_key(entity, K_UP))
            return entity_server_send_key(entity, K_RIGHT);
        else
            return 0;
    }
    if (x > cur_x && y > cur_y)
    {
        if (entity_server_send_key(entity, K_RIGHT))
            return entity_server_send_key(entity, K_DOWN);
        else
            return 0;
    }

    if (x < cur_x) return entity_server_send_key(entity, K_LEFT);
    if (x > cur_x) return entity_server_send_key(entity, K_RIGHT);
    if (y < cur_y) return entity_server_send_key(entity, K_UP);
    if (y > cur_y) return entity_server_send_key(entity, K_DOWN);

    return 1;
}

int beast_map_look_x(const char *map, int start_x, int start_y, int offset_x)
{
    int size_x = PLAYER_FOV_X;

    while (offset_x != 0)
    {
        if (start_x + offset_x < 0 || start_x + offset_x >= PLAYER_FOV_X)
        {
            offset_x += (offset_x < 0) ? 1 : -1;
            continue;
        }

        char temp = map[start_y * size_x + start_x + offset_x];

        if (temp >= 1 && temp <= MAX_PLAYERS)
            return offset_x;

        offset_x += (offset_x < 0) ? 1 : -1;
    }

    return 0;
}

int beast_map_look_y(const char *map, int start_x, int start_y, int offset_y)
{
    int size_x = PLAYER_FOV_X;

    while (offset_y != 0)
    {
        if (start_y + offset_y < 0 || start_y + offset_y >= PLAYER_FOV_Y)
        {
            offset_y += (offset_y < 0) ? 1 : -1;
            continue;
        }

        char temp = map[(start_y + offset_y) * size_x + start_x];

        if (temp >= 1 && temp <= MAX_PLAYERS)
            return offset_y;

        offset_y += (offset_y < 0) ? 1 : -1;
    }

    return 0;
}

int entity_server_send_key(EntityInfo *entity, KeyCode key)
{
    entity->key = key;

    sem_post(&entity->client_answer);
    sem_wait(&entity->server_answer);

    if (entity->action_status == ACTION_ACCEPTED) return 0;

    return 1;
}

int entity_server_status_check(EntityInfo *entity)
{
    if (kill(entity->server_PID, 0) == -1) return 1;

    return 0;
}
