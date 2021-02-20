#include <stdio.h>
#include <ncurses.h>

#include "blame_maintain.h"

#define MAX_PROCESSES 1 + MAX_PLAYERS // 1 Serwer, 1 Bestie, 4 Graczy

int main()
{
    sem_t *main_sem = sem_open("SO2_GAME_PROCESS_QUEUE", O_CREAT, 0777, MAX_PROCESSES);

    int sem_value;
    sem_getvalue(main_sem, &sem_value);

    if (sem_value == 0)
    {
        printf("Server is full\n");
        getchar();
        return 0;
    }

    sem_wait(main_sem);
    sem_getvalue(main_sem, &sem_value);

    if (sem_value == MAX_PROCESSES - 1)
    {
        ServerInfo *server;

        server = server_init();
        maintain_server(server);
        server_destroy(server);

        sem_unlink("SO2_GAME_PROCESS_QUEUE");
    }
    else
    {
        int option = 1;
        printf("Set up player (1), bot (2) or beasts (3)?\n");
        while (scanf("%d", &option) != 1 || option < 1 || option > 3);

        EntityInfo *entity;

        entity = entity_init(option);
        maintain_entity(entity);
        entity_destroy(entity);

        sem_post(main_sem);
    }

    printf("Disconnected correctly\n");

    return 0;
}
