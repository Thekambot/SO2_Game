cmake_minimum_required(VERSION 3.16)
project(SO2_Game C)

set(CMAKE_C_STANDARD 99)

#add_link_options(-lcurses)
add_link_options(-pthread)

#set(CMAKE_CXX_FLAGS "-lcurses")
#target_link_libraries(ncurses)

add_executable(SO2_Game main.c blame.h blame/b_entity_player.c blame/b_entity_bot.c blame/b_entity_beast.c blame/b_server.c blame/b_internal_structs.h blame/b_internal_enums.h)

target_link_libraries(SO2_Game ncurses)
target_link_libraries(SO2_Game rt)
