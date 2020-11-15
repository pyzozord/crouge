/*
    .########..##....##.########..#######..########..#######..########..########.
    .##.....##..##..##.......##..##.....##......##..##.....##.##.....##.##.....##
    .##.....##...####.......##...##.....##.....##...##.....##.##.....##.##.....##
    .########.....##.......##....##.....##....##....##.....##.########..##.....##
    .##...........##......##.....##.....##...##.....##.....##.##...##...##.....##
    .##...........##.....##......##.....##..##......##.....##.##....##..##.....##
    .##...........##....########..#######..########..#######..##.....##.########.

    - kudos for the grafitti to notchris 
*/

#include <libtcod.h>
#include <libtcod/tileset/truetype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h> /* for NULL */
#include <string.h>

#include "mem.h"

#define CONSOLE_WIDTH 120
#define CONSOLE_HEIGHT 60
#define GLYPH_W 30
#define GLYPH_H 30

struct mem_freelist *mem;

unsigned screen_w;
unsigned screen_h;

TCOD_console_t backbuffer;
TCOD_key_t key = {TCODK_NONE, 0};
TCOD_mouse_t mouse;

void init_tcod() {
	int sw, sh;
	TCOD_sys_get_current_resolution(&sw, &sh);
	screen_w = sw / GLYPH_W;
	screen_h = sh / GLYPH_H;

	TCOD_tileset_load_truetype_("/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf", GLYPH_W, GLYPH_H);

	if (TCOD_console_init_root(screen_w, screen_h, "CROGUE", false, TCOD_RENDERER_OPENGL2)) {
		fprintf(stderr, "libtcod error: %s\n", TCOD_get_error());
		exit(1);
	}

	atexit(TCOD_quit);

	TCOD_sys_set_fps(60);
	backbuffer = TCOD_console_new(screen_w, screen_h);
}

void game_loop() {
	int x=0, y=0;
	do {
		TCOD_console_clear(backbuffer);

		// render game
		TCOD_console_printf(backbuffer, x, y, "@");

		// render ui
		TCOD_console_printf(backbuffer, 0, screen_h-3, "last frame  : %3d ms (%3d fps)", (int)(TCOD_sys_get_last_frame_length() * 1000), TCOD_sys_get_fps());
		TCOD_console_printf(backbuffer, 0, screen_h-2, "elapsed	    : %8dms %4.2fs", TCOD_sys_elapsed_milli(), TCOD_sys_elapsed_seconds());
		TCOD_console_printf(backbuffer, 0, screen_h-1, "ALT-ENTER : switch to %s", TCOD_console_is_fullscreen() ? "windowed mode  " : "fullscreen mode");

		// print to root console
		TCOD_console_blit( backbuffer, 0, 0, screen_w, screen_h, NULL, 0, 0, 1.0f, 1.0f);

		// render, sync fps
		TCOD_console_flush();

		// handle input
		TCOD_sys_check_for_event((TCOD_event_t)(TCOD_EVENT_KEY_PRESS | TCOD_EVENT_MOUSE), &key, &mouse);

		if (key.vk == TCODK_TEXT && key.text[0] == 'h') {
			x--;
		} else if (key.vk == TCODK_TEXT && key.text[0] == 'j') {
			y++;
		} else if (key.vk == TCODK_TEXT && key.text[0] == 'k') {
			y--;
		} else if (key.vk == TCODK_TEXT && key.text[0] == 'l') {
			x++;
		} else if (key.vk == TCODK_TEXT && key.text[0] == 'q') {
			return;
		} else if (key.vk == TCODK_ENTER && (key.lalt | key.ralt)) {
			/* ALT-ENTER : switch fullscreen */
			TCOD_console_set_fullscreen(!TCOD_console_is_fullscreen());
		}
	} while (!TCOD_console_is_window_closed());
}

int main(int argc, char* argv[]) {
	mem = mem_freelist_init(malloc(1 * GB), 1 * GB); 

	init_tcod();

	game_loop();

	return 0;
}
