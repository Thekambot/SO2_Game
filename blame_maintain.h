#ifndef SO2_GAME_BLAME_MAINTAIN_H
#define SO2_GAME_BLAME_MAINTAIN_H

#include "blame.h"

// Gotowe funkcje do main

void        maintain_server(ServerInfo *);
void        maintain_entity(EntityInfo *);

void        *maintain_stdin_controller(void *arg);

KeyCode     maintain_int_to_KeyCode(int key);

void        maintain_map_print(int y, int x, WINDOW *, MapData *);
void        maintain_map_add_symbol(MapData *, int);
void        maintain_server_info_print(int y, int x, WINDOW *, ServerInfo *);
void        maintain_entity_info_print(int y, int x, WINDOW *window, EntityInfo *entity);

#endif //SO2_GAME_BLAME_MAINTAIN_H
