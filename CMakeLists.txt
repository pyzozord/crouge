cmake_minimum_required(VERSION 3.16)
project(crogue VERSION 0.0.0 LANGUAGES C)

if(CMAKE_COMPILER_IS_GNUCC)
	add_compile_options(-pedantic-errors -g)
endif()

# add_executable(test src/mem.c)

set(
	sources
	src/mem.c
	src/main.c
)

add_executable(game ${sources})

set_target_properties(
	game 
	PROPERTIES
	C_STANDARD 11
	C_EXTENSIONS ON
)

# TODO: maybe do find_package() instead and remove vendors/libtcod
# apparently we should be building stuff in vendors ourselves and not relay
# on existing build systems in a vendor's project
# or use https://github.com/TheLartians/CPM.cmake

target_include_directories(game PRIVATE vendors/libtcod/src/)
target_link_directories(game PRIVATE vendors/libtcod/buildsys/autotools/.libs/)

target_link_libraries(game PRIVATE m tcod)
