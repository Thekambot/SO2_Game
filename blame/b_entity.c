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
