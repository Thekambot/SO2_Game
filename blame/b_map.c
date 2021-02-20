#ifndef SO2_GAME_B_MAP_C
#define SO2_GAME_B_MAP_C

#include <stdio.h>
#include <stdlib.h>

#include "../blame.h"

MapData *map_load(char *filename)
{
    FILE *file = fopen(filename, "r, ccs=utf-8");
    if (file == NULL)
        return NULL;

    MapData *map = (MapData *) calloc(1, sizeof(MapData));
    if (map == NULL)
    {
        fclose(file);
        return NULL;
    }

    fscanf(file,"%d %d\n", &map->size_x, &map->size_y);
    fscanf(file,"%d %d\n", &map->camp_x, &map->camp_y);

    map->map = (char *) calloc((map->size_x * map->size_y) + 1, sizeof(char));
    if (map->map == NULL)
    {
        map_destroy(map);
        fclose(file);
        return NULL;
    }

    int i = 0;
    while (!feof(file))
    {
        char temp = (char) getc(file);

        if (temp == '\n' || temp == '\r')
            continue;

        map->map[i++] = temp;
    }

    fclose(file);
    return map;
}

void map_destroy(MapData *map)
{
//    for (int i = 0; i < map->camp_y; ++i)
//    {
//        free(map->map[i]);
//    }

    free(map->map);
    free(map);
}

int map_random_empty_location(MapData *map, int *x, int *y)
{
    int _x, _y;

    // todo: Check if there any empty spaces

    while (1)
    {
        _x = rand() % ((map->size_x - 1) + 1);
        _y = rand() % ((map->size_y - 1) + 1);

        if (map->map[_y * map->size_x + _x] == ' ')
            break;
    }

    *x = _x;
    *y = _y;

    return 0;
}

void map_random_generate_symbol(MapData *map, int symbol)
{
    int x, y;

    map_random_empty_location(map, &x, &y);

    map->map[y * map->size_x + x] = (char) symbol;
}

int map_copy_fragment(MapData *src, int x, int y, MapData *dest)
{
    if (dest->map == NULL)
    {
        dest->map = (char *) calloc(src->size_x * src->size_y, sizeof(char));
        if (dest->map == NULL)
            return -1;
        dest->size_x = src->size_x;
        dest->size_y = src->size_y;
    }

    for (int i = 0; i < dest.; ++i)
    {

    }


}

#endif //SO2_GAME_B_MAP_C
