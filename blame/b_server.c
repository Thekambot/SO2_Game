#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "../blame.h"

ServerInfo *server_init()
{
    ServerInfo *server = (ServerInfo *)calloc(1, sizeof(ServerInfo));
    if (server == NULL)
        return NULL;

    server->this_PID = getpid();

    server->map = map_load("default_map.txt");

    return server;
}

void server_destroy(ServerInfo *ptr)
{
    map_destroy(ptr->map);

    free(ptr);
}
