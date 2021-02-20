cmake_minimum_required(VERSION 3.16)
project(SO2_Game C)

set(CMAKE_C_STANDARD 99)

#add_link_options(-lcurses)
add_link_options(-pthread)
add_link_options(-lncursesw)

#set(CMAKE_CXX_FLAGS "-lcurses")
#target_link_libraries(ncurses)

add_executable(SO2_Game main.c blame.h blame/b_server.c blame/b_internal_structs.h blame/b_internal_enums.h blame/b_map.c blame/b_entity.c blame_maintain.c blame_maintain.h)

target_link_libraries(SO2_Game ncurses)
target_link_libraries(SO2_Game rt)
