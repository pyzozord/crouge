cmake_minimum_required(VERSION 3.16)
project(crogue VERSION 0.0.0 LANGUAGES C)

set(
	sources
	src/main.c
)

add_executable(game ${sources})

set_target_properties(
	game 
	PROPERTIES
	C_STANDARD 11
	C_EXTENSIONS ON
)

if(CMAKE_COMPILER_IS_GNUCC)
	target_compile_options(game PRIVATE -pedantic-errors)
endif()

target_link_libraries(game m tcod SDL2)
