#include <stdio.h>
#include <ncurses.h>
#include <locale.h>

#include "blame_maintain.h"

void maintain_server(ServerInfo *server)
{
    setlocale(LC_ALL, "");

    initscr();
    cbreak();
    noecho();

    WINDOW *map = newwin(server->map->size_y, server->map->size_x, 0, 0);
    WINDOW *debug = newwin(0, 0, server->map->size_y + 5, 0);
    WINDOW *info = newwin(100, 200, 0, server->map->size_x + 5);

    while (1)
    {
        maintain_map_print(0, 0, map, server->map);
        wrefresh(map);

        maintain_server_info_print(0, 0, info, server);
        wrefresh(info);

        int pressed_key = getch();
        mvwprintw(debug, 0, 0, "Button pressed: %lc", (unsigned char)pressed_key);
        wrefresh(debug);
        if (pressed_key == 'q')
            break;
        maintain_map_add_symbol(server->map, pressed_key);

    }

    endwin();
}

void maintain_entity(EntityInfo *entity)
{
    if (entity->entity_type == ENTITY_HUMAN)
        printf("Siema tu gracz\n");
    else if (entity->entity_type == ENTITY_BOT)
        printf("Siema tu bot\n");

    getchar();
}

void  maintain_map_print(int y, int x, WINDOW *window, MapData *map)
{
    for (int i = 0; i < map->size_y; ++i)
    {
        for (int j = 0; j < map->size_x; ++j)
        {
//            if (*(map->map + i * map->size_x + j) == ':')
//                mvwprintw(window, i + y, j + x, "%lc", L'█');
//            else
                mvwprintw(window, i + y, j + x, "%c", *(map->map + i * map->size_x + j));
        }
    }
}

void maintain_map_add_symbol(MapData *map, int key)
{
    if (!SERVER_LEGAL_KEYS(key))
        return;
    map_random_generate_symbol(map, key);
}

void maintain_server_info_print(int y, int x, WINDOW *window, ServerInfo *server)
{
    mvwprintw(window, y++, x, "Server's PID: %d", server->this_PID);
    mvwprintw(window, y++, x + 1, "Campsite X/Y: %d/%d", server->map->camp_x, server->map->camp_y);
    mvwprintw(window, y++, x + 1, "Round number: x");

    mvwprintw(window, y++, x,       "Parameter:   ");
    mvwprintw(window, y++, x + 1,   "PID:          ");
    mvwprintw(window, y++, x + 1,   "Type:         ");
    mvwprintw(window, y++, x + 1,   "Curr X/Y:     ");
    mvwprintw(window, y++, x + 1,   "Deaths:       ");
    y++;
    mvwprintw(window, y++, x,       "Coins        ");
    mvwprintw(window, y++, x + 1,   "carried       ");
    mvwprintw(window, y++, x + 1,   "brought       ");
    y -= 9;

    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        x = strlen("Parameter:   ");
        x += (i + 1) * (strlen("Player") + 2);

        mvwprintw(window, y++, x, "Player%d", i + 1);

        if (server->entities[i] != NULL)
        {
            mvwprintw(window, y++, x, "%d", server->entities[i]->this_PID);
            mvwprintw(window, y++, x, "%s", server->entities[i]->entity_type == ENTITY_HUMAN ? "HUMAN" : "BOT");
            mvwprintw(window, y++, x, "%d/%d", server->entities[i]->cur_x, server->entities[i]->cur_y);
            mvwprintw(window, y++, x, "%d", server->entities[i]->death_counter);
            y += 2;
            mvwprintw(window, y++, x, "%d", server->entities[i]->coins_found_counter);
            mvwprintw(window, y++, x, "%d", server->entities[i]->coins_bank_counter);
            y -= 4;
        }
        else
        {
            mvwprintw(window, y++, x, "-");
            mvwprintw(window, y++, x, "-");
            mvwprintw(window, y++, x, "-/-");
            mvwprintw(window, y++, x, "-");
        }
        y -= 5;
    }
}
