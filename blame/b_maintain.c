#include <stdio.h>

#include "../blame.h"

void server_maintain(ServerInfo *server)
{
    printf("Siema tu serwer\n");

    getchar();
}

void entity_maintain(EntityInfo *entity)
{
    if (entity->entity_type == ENTITY_PLAYER)
        printf("Siema tu gracz\n");
    else if (entity->entity_type == ENTITY_BOT)
        printf("Siema tu bot\n");

    getchar();
}