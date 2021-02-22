#include <stdio.h>
#include <ncurses.h>

#include "blame_maintain.h"

#define MAX_PROCESSES 1 + MAX_PLAYERS // 1 Serwer, 1 Bestie, 4 Graczy
#define MAIN_SEM_NAME "SO2_GAME_SERVER_STATUS"

int main()
{
    srand(time(NULL));

    sem_t *main_sem = sem_open(MAIN_SEM_NAME, O_CREAT, 0777, 0);

    int sem_value;
    sem_getvalue(main_sem, &sem_value);

    if (sem_value == 0)
    {
        ServerInfo *server;

        server = server_init();
        sem_post(main_sem);

        maintain_server(server);
        server_destroy(server);

        sem_unlink(MAIN_SEM_NAME);
    }
    else
    {
//        wint_t option_char = 1;
//        printf("Set up player (1), bot (2)\n");
//        option_char = getwc(stdin);
//        if (option_char != '1' && option_char != '2') return 1;
//
//        int option = (int) option_char - '0';

        int option = 1;
        EntityInfo *entity;

        entity = entity_server_connection_start(option);
        if (entity == (void *)-1)
        {
            printf("Server rejected request\n");
            return 0;
        }
        if (entity == (void *)-2)
        {
            printf("Server does not exist\n");
            return 0;
        }
        if (entity == (void *)-2)
        {
            printf("Sem does not exist\n");
            return 0;
        }
        if (entity == NULL)
        {
            printf("Error\n");
            return 0;
        }

        maintain_entity(entity);
        entity_server_connection_end(entity);
    }

    printf("Disconnected correctly\n");

    return 0;
}
