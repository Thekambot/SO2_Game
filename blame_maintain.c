#define _GNU_SOURCE

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

    pthread_t stdinController;
    pthread_create(&stdinController, NULL, maintain_stdin_controller, NULL);
    int *pressed_key;

    while (1)
    {
        werase(map);
        maintain_map_print(0, 0, map, server->map);
        maintain_map_print(0, 0, map, server->map_entities);
        wrefresh(map);

        werase(info);
        maintain_server_info_print(0, 0, info, server);
        wrefresh(info);

        //server_entity_thread_function(server);

        if (pthread_tryjoin_np(stdinController, (void *) &pressed_key) == 0)
        {
            char *decoded = (char *) pressed_key;
            mvwprintw(debug, 0, 0, "Button pressed: %02x %02x %02x %02x", decoded[0], decoded[1], decoded[2], decoded[3]);
            wrefresh(debug);

            if (*pressed_key == 'q' || *pressed_key == 'Q')
            {
                free(pressed_key);
                break;
            }

            maintain_map_add_symbol(server->map, *pressed_key);
            free(pressed_key);

            pthread_create(&stdinController, NULL, maintain_stdin_controller, NULL);
        }

    }

    endwin();
}

void maintain_entity(EntityInfo *entity)
{
    pid_t server_pid = entity->server_PID;

    setlocale(LC_ALL, "");

    initscr();
    cbreak();
    noecho();

    WINDOW *map = newwin(PLAYER_FOV_Y, PLAYER_FOV_X, 0, 0);
    WINDOW *debug = newwin(0, 0, PLAYER_FOV_Y + 15, 0);
    WINDOW *info = newwin(100, 200, 0, PLAYER_FOV_X + 30);

    MapData fov;
    fov.map = entity->fov;
    fov.size_x = PLAYER_FOV_X;
    fov.size_y = PLAYER_FOV_Y;

    pthread_t stdinController;
    pthread_create(&stdinController, NULL, maintain_stdin_controller, NULL);
    int *pressed_key;

    while (1)
    {
        if (kill(server_pid, 0) == -1)
        {
            mvwprintw(debug, 0, 0, "Server disconnected, press any key to exit\n");
            wrefresh(debug);
            pthread_join(stdinController, (void *) &pressed_key);
            free(pressed_key);
            break;
        }

        werase(map);
        maintain_map_print(0, 0, map, &fov);
        wrefresh(map);

        werase(info);
        maintain_entity_info_print(0, 0, info, entity);
        wrefresh(info);

        if (pthread_tryjoin_np(stdinController, (void *) &pressed_key) == 0)
        {
            char *decoded = (char *) pressed_key;
            mvwprintw(debug, 0, 0, "Button pressed: %02x %02x %02x %02x", decoded[0], decoded[1], decoded[2], decoded[3]);
            wrefresh(debug);

            KeyCode key = maintain_int_to_KeyCode(*pressed_key);
            mvwprintw(debug, 1, 0, "Key Code: %d", key);
            wrefresh(debug);

            if (kill(server_pid, 0) == -1)
            {
                mvwprintw(debug, 0, 0, "Server disconnected, press any key to exit\n");
                wrefresh(debug);
                free(pressed_key);
                break;
            }

            int test = entity_server_send_key(entity, key);
            mvwprintw(debug, 2, 0, "%s", test ? "Key rejected by server" : "Key accepted by server");
            wrefresh(debug);

            if (*pressed_key == 'q' || *pressed_key == 'Q')
            {
                free(pressed_key);
                break;
            }

            free(pressed_key);

            pthread_create(&stdinController, NULL, maintain_stdin_controller, NULL);
        }

    }

    endwin();
}

void *maintain_stdin_controller(void *arg)
{
    wint_t *c = calloc(1, sizeof(wint_t));

    char *ptr = (char *) c;

    ptr[0] = getwc(stdin);

    if (ptr[0] == 0x1B)
    {
        ptr[1] = getwc(stdin);
        ptr[2] = getwc(stdin);
    }

    return c;
}

KeyCode maintain_int_to_KeyCode(int key)
{
    char *ptr = (char *) &key;
    char res;

    if (ptr[0] == 0x1B) res = ptr[2];
    else res = ptr[0];

    switch (res)
    {
        case 0x44:  return K_LEFT;
        case 0x43:  return K_RIGHT;
        case 0x41:  return K_UP;
        case 0x42:  return K_DOWN;

        case 'q':
        case 'Q':   return K_QUIT;

        default:    return K_NONE;
    }
}

void  maintain_map_print(int y, int x, WINDOW *window, MapData *map)
{
    for (int i = 0; i < map->size_y; ++i)
    {
        for (int j = 0; j < map->size_x; ++j)
        {

            char object = *(map->map + i * map->size_x + j);
            if (object == '\0') continue;

            if (object > 0 && object <= MAX_PLAYERS)
                mvwprintw(window, i + y, j + x, "%c", object + '0');
            else if (object > MAX_PLAYERS && object <= MAX_ENTITIES)
                mvwprintw(window, i + y, j + x, "%c", '*');
            else if (object < 0)
                mvwprintw(window, i + y, j + x, "%c", 'D');
            else
                mvwprintw(window, i + y, j + x, "%c", object);
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
    mvwprintw(window, y++, x + 1, "Round number: %d", server->round_count);

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
        
        EntityInfo *entity = NULL;
        if (server->shm_entities[i] != NULL)
            entity = (EntityInfo *) server->shm_entities[i]->memory_map;

        if (entity != NULL)
        {
            mvwprintw(window, y++, x, "%d", entity->this_PID);
            mvwprintw(window, y++, x, "%s", entity->entity_type == ENTITY_HUMAN ? "HUMAN" : "BOT");
            mvwprintw(window, y++, x, "%d/%d", entity->cur_x, entity->cur_y);
            mvwprintw(window, y++, x, "%d", entity->death_counter);
            y += 2;
            mvwprintw(window, y++, x, "%d", entity->coins_found_counter);
            mvwprintw(window, y++, x, "%d", entity->coins_bank_counter);
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

void maintain_entity_info_print(int y, int x, WINDOW *window, EntityInfo *entity)
{
    mvwprintw(window, y++, x, "Player's PID: %d", entity->this_PID);
    mvwprintw(window, y++, x + 1, "Campsite X/Y: UNKNOWN");
    mvwprintw(window, y++, x + 1, "Round number: %d", entity->round);
    y++;

    mvwprintw(window, y++, x, "Player:");
    mvwprintw(window, y++, x + 1, "Number:");
    mvwprintw(window, y++, x + 1, "Type:");
    mvwprintw(window, y++, x + 1, "Curr X/Y:");
    mvwprintw(window, y++, x + 1, "Deaths:");
    y++;
    mvwprintw(window, y++, x + 1, "Coins found");
    mvwprintw(window, y++, x + 1, "Coins brought");
    y -= 7;
    x += 15;

    mvwprintw(window, y++, x, "%d", entity->player_number);
    mvwprintw(window, y++, x, "%s", entity->entity_type == ENTITY_HUMAN ? "HUMAN" : "BOT");
    mvwprintw(window, y++, x, "%d/%d", entity->cur_x, entity->cur_y);
    mvwprintw(window, y++, x, "%d", entity->death_counter);
    y++;
    mvwprintw(window, y++, x, "%d", entity->coins_found_counter);
    mvwprintw(window, y++, x, "%d", entity->coins_bank_counter);

}
