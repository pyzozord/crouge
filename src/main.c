#include <libtcod.h>
#include <libtcod/tileset/truetype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h> /* for NULL */
#include <string.h>

/* sample screen position */
#define SAMPLE_SCREEN_X 0
#define SAMPLE_SCREEN_Y 0

int CONSOLE_WIDTH = 120;
int CONSOLE_HEIGHT = 60;

int SAMPLE_SCREEN_WIDTH  = 120;
int SAMPLE_SCREEN_HEIGHT = 60;

TCOD_console_t sample_console;

void render_fov(TCOD_key_t* key) {
	// clang-format off
	static char* s_map[] = {
			"##############################################",
			"#######################      #################",
			"#####################    #     ###############",
			"######################  ###        ###########",
			"##################      #####             ####",
			"################       ########    ###### ####",
			"###############      #################### ####",
			"################    ######                  ##",
			"########   #######  ######   #     #     #  ##",
			"########   ######      ###                  ##",
			"########                                    ##",
			"####       ######      ###   #     #     #  ##",
			"#### ###   ########## ####                  ##",
			"#### ###   ##########   ###########=##########",
			"#### ##################   #####          #####",
			"#### ###             #### #####          #####",
			"####           #     ####                #####",
			"########       #     #### #####          #####",
			"########       #####      ####################",
			"##############################################",
	};
	// clang-format on
#define TORCH_RADIUS 10.0f
#define SQUARED_TORCH_RADIUS (TORCH_RADIUS * TORCH_RADIUS)
	static int px = 20, py = 10; /* player position */
	static bool recompute_fov = true;
	static bool torch = false;
	static bool light_walls = true;
	static TCOD_map_t map = NULL;
	static TCOD_color_t dark_wall = {0, 0, 100};
	static TCOD_color_t light_wall = {130, 110, 50};
	static TCOD_color_t dark_ground = {50, 50, 150};
	static TCOD_color_t light_ground = {200, 180, 50};
	static TCOD_noise_t noise;
	static int algonum = 0;
	static char* algo_names[] = {
			"BASIC               ",
			"DIAMOND             ",
			"SHADOW              ",
			"PERMISSIVE0         ",
			"PERMISSIVE1         ",
			"PERMISSIVE2         ",
			"PERMISSIVE3         ",
			"PERMISSIVE4         ",
			"PERMISSIVE5         ",
			"PERMISSIVE6         ",
			"PERMISSIVE7         ",
			"PERMISSIVE8         ",
			"RESTRICTIVE         ",
			"SYMMETRIC_SHADOWCAST",
	};
	static float torch_x = 0.0f; /* torch light position in the perlin noise */

	int mapw = (int) fmin(SAMPLE_SCREEN_WIDTH, strlen(s_map[0]));
	int maph = (int) fmin(SAMPLE_SCREEN_HEIGHT, sizeof(s_map)/sizeof(s_map[0]));

	int x, y;
	/* torch position & intensity variation */
	float dx = 0.0f, dy = 0.0f, di = 0.0f;
	if (!map) {
		map = TCOD_map_new(SAMPLE_SCREEN_WIDTH, SAMPLE_SCREEN_HEIGHT);
		for (y = 0; y < maph; y++) {
			for (x = 0; x < mapw; x++) {
				if (s_map[y][x] == ' ')
					TCOD_map_set_properties(map, x, y, true, true); /* ground */
				else if (s_map[y][x] == '=')
					TCOD_map_set_properties(map, x, y, true, false); /* window */
			}
		}
		noise = TCOD_noise_new(1, 1.0f, 1.0f, NULL); /* 1d noise for the torch flickering */
	}

	/* we draw the foreground only the first time.
	during the player movement, only the @ is redrawn.
	the rest impacts only the background color */

	/* draw the help text & player @ */
	TCOD_console_clear(sample_console);
	TCOD_console_set_default_foreground(sample_console, TCOD_white);
	TCOD_console_printf(
			sample_console,
			0,
			0,
			"IJKL : move around\nT : torch fx %s\nW : light walls %s\n+-: algo %s",
			torch ? "on " : "off",
			light_walls ? "on " : "off",
			algo_names[algonum]);
	TCOD_console_set_default_foreground(sample_console, TCOD_black);
	TCOD_console_put_char(sample_console, px, py, '@', TCOD_BKGND_NONE);
	/* draw windows */

	for (y = 0; y < maph; y++) {
		for (x = 0; x < mapw; x++) {
			if (s_map[y][x] == '=') {
				TCOD_console_put_char(sample_console, x, y, TCOD_CHAR_DHLINE, TCOD_BKGND_NONE);
			}
		}
	}

	if (recompute_fov) {
		recompute_fov = false;
		TCOD_map_compute_fov(map, px, py, torch ? (int)(TORCH_RADIUS) : 0, light_walls, (TCOD_fov_algorithm_t)algonum);
	}
	if (torch) {
		float tdx;
		/* slightly change the perlin noise parameter */
		torch_x += 0.2f;
		/* randomize the light position between -1.5 and 1.5 */
		tdx = torch_x + 20.0f;
		dx = TCOD_noise_get(noise, &tdx) * 1.5f;
		tdx += 30.0f;
		dy = TCOD_noise_get(noise, &tdx) * 1.5f;
		di = 0.2f * TCOD_noise_get(noise, &torch_x);
	}
	for (y = 0; y < maph; y++) {
		for (x = 0; x < mapw; x++) {
			bool visible = TCOD_map_is_in_fov(map, x, y);
			bool wall = s_map[y][x] == '#';
			if (!visible) {
				TCOD_console_set_char_background(sample_console, x, y, wall ? dark_wall : dark_ground, TCOD_BKGND_SET);

			} else {
				if (!torch) {
					TCOD_console_set_char_background(sample_console, x, y, wall ? light_wall : light_ground, TCOD_BKGND_SET);
				} else {
					TCOD_color_t base = (wall ? dark_wall : dark_ground);
					TCOD_color_t light = (wall ? light_wall : light_ground);
					float r =
							(x - px + dx) * (x - px + dx) + (y - py + dy) * (y - py + dy); /* cell distance to torch (squared) */
					if (r < SQUARED_TORCH_RADIUS) {
						float l = (SQUARED_TORCH_RADIUS - r) / SQUARED_TORCH_RADIUS + di;
						l = CLAMP(0.0f, 1.0f, l);
						base = TCOD_color_lerp(base, light, l);
					}
					TCOD_console_set_char_background(sample_console, x, y, base, TCOD_BKGND_SET);
				}
			}
		}
	}
	if (key->vk == TCODK_TEXT && key->text[0] != '\0') {
		if (key->text[0] == 'I' || key->text[0] == 'i') {
			if (s_map[py - 1][px] == ' ') {
				TCOD_console_put_char(sample_console, px, py, ' ', TCOD_BKGND_NONE);
				py--;
				TCOD_console_put_char(sample_console, px, py, '@', TCOD_BKGND_NONE);
				recompute_fov = true;
			}
		} else if (key->text[0] == 'K' || key->text[0] == 'k') {
			if (s_map[py + 1][px] == ' ') {
				TCOD_console_put_char(sample_console, px, py, ' ', TCOD_BKGND_NONE);
				py++;
				TCOD_console_put_char(sample_console, px, py, '@', TCOD_BKGND_NONE);
				recompute_fov = true;
			}
		} else if (key->text[0] == 'J' || key->text[0] == 'j') {
			if (s_map[py][px - 1] == ' ') {
				TCOD_console_put_char(sample_console, px, py, ' ', TCOD_BKGND_NONE);
				px--;
				TCOD_console_put_char(sample_console, px, py, '@', TCOD_BKGND_NONE);
				recompute_fov = true;
			}
		} else if (key->text[0] == 'L' || key->text[0] == 'l') {
			if (s_map[py][px + 1] == ' ') {
				TCOD_console_put_char(sample_console, px, py, ' ', TCOD_BKGND_NONE);
				px++;
				TCOD_console_put_char(sample_console, px, py, '@', TCOD_BKGND_NONE);
				recompute_fov = true;
			}
		} else if (key->text[0] == 'T' || key->text[0] == 't') {
			torch = !torch;
			TCOD_console_set_default_foreground(sample_console, TCOD_white);
			TCOD_console_printf(
					sample_console,
					1,
					0,
					"IJKL : move around\nT : torch fx %s\nW : light walls %s\n+-: algo %s",
					torch ? "on " : "off",
					light_walls ? "on " : "off",
					algo_names[algonum]);
			TCOD_console_set_default_foreground(sample_console, TCOD_black);
		} else if (key->text[0] == 'W' || key->text[0] == 'w') {
			light_walls = !light_walls;
			TCOD_console_set_default_foreground(sample_console, TCOD_white);
			TCOD_console_printf(
					sample_console,
					1,
					0,
					"IJKL : move around\nT : torch fx %s\nW : light walls %s\n+-: algo %s",
					torch ? "on " : "off",
					light_walls ? "on " : "off",
					algo_names[algonum]);
			TCOD_console_set_default_foreground(sample_console, TCOD_black);
			recompute_fov = true;
		} else if (key->text[0] == '+' || key->text[0] == '-') {
			algonum += key->text[0] == '+' ? 1 : -1;
			algonum = (algonum + NB_FOV_ALGORITHMS) % NB_FOV_ALGORITHMS;
			TCOD_console_set_default_foreground(sample_console, TCOD_white);
			TCOD_console_printf(
					sample_console,
					1,
					0,
					"IJKL : move around\nT : torch fx %s\nW : light walls %s\n+-: algo %s",
					torch ? "on " : "off",
					light_walls ? "on " : "off",
					algo_names[algonum]);
			TCOD_console_set_default_foreground(sample_console, TCOD_black);
			recompute_fov = true;
		}
	}
}

#define GLYPH_W 30
#define GLYPH_H 30

/* ***************************
 * the main function
 * ***************************/
int main(int argc, char* argv[]) {
	int sw, sh;
	TCOD_key_t key = {TCODK_NONE, 0};
        TCOD_mouse_t mouse;

	//TCOD_console_set_custom_font(font, font_flags, nb_char_horiz, nb_char_vertic);
	TCOD_tileset_load_truetype_("/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf", GLYPH_W, GLYPH_H);

	//TCOD_sys_force_fullscreen_resolution(1280, 720);

	TCOD_sys_get_current_resolution(&sw, &sh);

	SAMPLE_SCREEN_WIDTH = CONSOLE_WIDTH = sw / GLYPH_W;
	SAMPLE_SCREEN_HEIGHT = CONSOLE_HEIGHT = sh / GLYPH_H;

	if (TCOD_console_init_root(CONSOLE_WIDTH, CONSOLE_HEIGHT, "libtcod C sample", false, TCOD_RENDERER_OPENGL2)) {
		fprintf(stderr, "libtcod error: %s\n", TCOD_get_error());
		return 1;
	}
	atexit(TCOD_quit);

	sample_console = TCOD_console_new(SAMPLE_SCREEN_WIDTH, SAMPLE_SCREEN_HEIGHT);

	TCOD_sys_set_fps(60);

	int i;
	do {
		// render game
		render_fov(&key);

		// render ui
		TCOD_console_set_default_foreground(sample_console, TCOD_grey);
		TCOD_console_printf(sample_console, 0, CONSOLE_HEIGHT-3, "last frame : %3d ms (%3d fps)", (int)(TCOD_sys_get_last_frame_length() * 1000), TCOD_sys_get_fps());
		TCOD_console_printf(sample_console, 0, CONSOLE_HEIGHT-2, "elapsed : %8dms %4.2fs", TCOD_sys_elapsed_milli(), TCOD_sys_elapsed_seconds());
		TCOD_console_printf(sample_console, 0, CONSOLE_HEIGHT-1, "ALT-ENTER : switch to %s", TCOD_console_is_fullscreen() ? "windowed mode  " : "fullscreen mode");

		// print to root console
		TCOD_console_blit( sample_console, 0, 0, SAMPLE_SCREEN_WIDTH, SAMPLE_SCREEN_HEIGHT, NULL, SAMPLE_SCREEN_X, SAMPLE_SCREEN_Y, 1.0f, 1.0f);

		// render, sync fps
		TCOD_console_flush();

		// handle input
		TCOD_sys_check_for_event((TCOD_event_t)(TCOD_EVENT_KEY_PRESS | TCOD_EVENT_MOUSE), &key, &mouse);

		if (key.vk == TCODK_TEXT && key.text[0] == 'q') {
			return 0;
		} else if (key.vk == TCODK_ENTER && (key.lalt | key.ralt)) {
			/* ALT-ENTER : switch fullscreen */
			TCOD_console_set_fullscreen(!TCOD_console_is_fullscreen());
		} else if (key.vk == TCODK_PRINTSCREEN) {
			if (key.lalt) {
				/* Alt-PrintScreen : save to samples.asc */
				TCOD_console_save_asc(NULL, "samples.asc");
			} else {
				/* save screenshot */
				TCOD_sys_save_screenshot(NULL);
			}
		}
	} while (!TCOD_console_is_window_closed());
	return 0;
}
