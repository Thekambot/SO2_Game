#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "../blame.h"

EntityInfo *entity_init(EntityType type)
{
    EntityInfo *entity = (EntityInfo *) calloc(1, sizeof(EntityInfo));
    if (entity == NULL)
        return NULL;

    // todo: server_info

    entity->entity_type = type;

    // todo: player number

    // todo: rand cur_x,y

    entity->death_counter = 0;
    entity->coins_found_counter = 0;
    entity->coins_bank_counter = 0;
}

void entity_destroy(EntityInfo *entity)
{
    free(entity);
}