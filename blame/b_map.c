#ifndef SO2_GAME_B_MAP_C
#define SO2_GAME_B_MAP_C

#include <stdio.h>
#include <stdlib.h>

#include "../blame.h"

MapData *map_load(char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
        return NULL;

    MapData *map = (MapData *) calloc(1, sizeof(MapData));
    if (map == NULL)
    {
        fclose(file);
        return NULL;
    }

    fscanf(file,"%d %d\n", &map->camp_x, &map->camp_y);
    fscanf(file,"%d %d\n", &map->size_x, &map->size_y);

    map->map = (char **) calloc(map->size_y + 1, sizeof(char *));
    if (map->map == NULL)
    {
        map_destroy(map);
        fclose(file);
        return NULL;
    }

    for (int i = 0; i < map->camp_y; ++i)
    {
        map->map[i] = (char *) calloc(map->size_x + 1, sizeof(char));
        if (map->map[i] == NULL)
        {
            map_destroy(map);
            fclose(file);
            return NULL;
        }

        fread(map->map[i], 1, map->size_x, file);
        *(map->map[i] + map->size_x) = '\0';
        fseek(file, 1, SEEK_CUR);
    }

    fclose(file);
    return map;
}

void map_destroy(MapData *map)
{
    for (int i = 0; i < map->camp_y; ++i)
    {
        free(map->map[i]);
    }

    free(map->map);
    free(map);
}

#endif //SO2_GAME_B_MAP_C
