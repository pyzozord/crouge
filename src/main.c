// uncomment this to disable SDL sample (might cause compilation issues on some systems)
//#define NO_SDL_SAMPLE

#include <SDL2/SDL.h>
#include <libtcod.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h> /* for NULL */
#include <string.h>

/* a sample has a name and a rendering function */
typedef struct {
  char name[64];
  void (*render)(bool first, TCOD_key_t* key, TCOD_mouse_t* mouse);
} sample_t;

/* sample screen size */
#define SAMPLE_SCREEN_WIDTH 46
#define SAMPLE_SCREEN_HEIGHT 20

/* sample screen position */
#define SAMPLE_SCREEN_X 20
#define SAMPLE_SCREEN_Y 10

/* ***************************
 * samples rendering functions
 * ***************************/

/* the offscreen console in which the samples are rendered */
TCOD_console_t sample_console;

/* ***************************
 * true colors sample
 * ***************************/
void render_colors(bool first, TCOD_key_t* key, TCOD_mouse_t* mouse) {
  enum { TOPLEFT, TOPRIGHT, BOTTOMLEFT, BOTTOMRIGHT };
  static TCOD_color_t cols[4] = {{50, 40, 150}, {240, 85, 5}, {50, 35, 240}, {10, 200, 130}}; /* random corner colors */
  static int dir_r[4] = {1, -1, 1, 1}, dir_g[4] = {1, -1, -1, 1}, dir_b[4] = {1, 1, 1, -1};
  int c, x, y;
  TCOD_color_t textColor;
  /* ==== slightly modify the corner colors ==== */
  if (first) {
    TCOD_console_clear(sample_console);
  }
  /* ==== slightly modify the corner colors ==== */
  for (c = 0; c < 4; c++) {
    /* move each corner color */
    int component = TCOD_random_get_int(NULL, 0, 2);
    switch (component) {
      case 0:
        cols[c].r += 5 * dir_r[c];
        if (cols[c].r == 255)
          dir_r[c] = -1;
        else if (cols[c].r == 0)
          dir_r[c] = 1;
        break;
      case 1:
        cols[c].g += 5 * dir_g[c];
        if (cols[c].g == 255)
          dir_g[c] = -1;
        else if (cols[c].g == 0)
          dir_g[c] = 1;
        break;
      case 2:
        cols[c].b += 5 * dir_b[c];
        if (cols[c].b == 255)
          dir_b[c] = -1;
        else if (cols[c].b == 0)
          dir_b[c] = 1;
        break;
    }
  }

  /* ==== scan the whole screen, interpolating corner colors ==== */
  for (x = 0; x < SAMPLE_SCREEN_WIDTH; x++) {
    float x_coef = (float)(x) / (SAMPLE_SCREEN_WIDTH - 1);
    /* get the current column top and bottom colors */
    TCOD_color_t top = TCOD_color_lerp(cols[TOPLEFT], cols[TOPRIGHT], x_coef);
    TCOD_color_t bottom = TCOD_color_lerp(cols[BOTTOMLEFT], cols[BOTTOMRIGHT], x_coef);
    for (y = 0; y < SAMPLE_SCREEN_HEIGHT; y++) {
      float y_coef = (float)(y) / (SAMPLE_SCREEN_HEIGHT - 1);
      /* get the current cell color */
      TCOD_color_t curColor = TCOD_color_lerp(top, bottom, y_coef);
      TCOD_console_set_char_background(sample_console, x, y, curColor, TCOD_BKGND_SET);
    }
  }

  /* ==== print the text ==== */
  /* get the background color at the text position */
  textColor = TCOD_console_get_char_background(sample_console, SAMPLE_SCREEN_WIDTH / 2, 5);
  /* and invert it */
  textColor.r = 255 - textColor.r;
  textColor.g = 255 - textColor.g;
  textColor.b = 255 - textColor.b;
  TCOD_console_set_default_foreground(sample_console, textColor);
  /* put random text (for performance tests) */
  for (x = 0; x < SAMPLE_SCREEN_WIDTH; x++) {
    for (y = 0; y < SAMPLE_SCREEN_HEIGHT; y++) {
      int c;
      TCOD_color_t col = TCOD_console_get_char_background(sample_console, x, y);
      col = TCOD_color_lerp(col, TCOD_black, 0.5f);
      c = TCOD_random_get_int(NULL, 'a', 'z');
      TCOD_console_set_default_foreground(sample_console, col);
      TCOD_console_put_char(sample_console, x, y, c, TCOD_BKGND_NONE);
    }
  }
  /* the background behind the text is slightly darkened using the BKGND_MULTIPLY flag */
  TCOD_console_set_default_background(sample_console, TCOD_grey);
  TCOD_console_printf_rect_ex(
      sample_console,
      SAMPLE_SCREEN_WIDTH / 2,
      5,
      SAMPLE_SCREEN_WIDTH - 2,
      SAMPLE_SCREEN_HEIGHT - 1,
      TCOD_BKGND_MULTIPLY,
      TCOD_CENTER,
      "The Doryen library uses 24 bits colors, for both background and foreground.");
}

/* ***************************
 * fov sample
 * ***************************/
void render_fov(bool first, TCOD_key_t* key, TCOD_mouse_t* mouse) {
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
  int x, y;
  /* torch position & intensity variation */
  float dx = 0.0f, dy = 0.0f, di = 0.0f;
  if (!map) {
    map = TCOD_map_new(SAMPLE_SCREEN_WIDTH, SAMPLE_SCREEN_HEIGHT);
    for (y = 0; y < SAMPLE_SCREEN_HEIGHT; y++) {
      for (x = 0; x < SAMPLE_SCREEN_WIDTH; x++) {
        if (s_map[y][x] == ' ')
          TCOD_map_set_properties(map, x, y, true, true); /* ground */
        else if (s_map[y][x] == '=')
          TCOD_map_set_properties(map, x, y, true, false); /* window */
      }
    }
    noise = TCOD_noise_new(1, 1.0f, 1.0f, NULL); /* 1d noise for the torch flickering */
  }
  if (first) {
    /* we draw the foreground only the first time.
       during the player movement, only the @ is redrawn.
       the rest impacts only the background color */
    /* draw the help text & player @ */
    TCOD_console_clear(sample_console);
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
    TCOD_console_put_char(sample_console, px, py, '@', TCOD_BKGND_NONE);
    /* draw windows */
    for (y = 0; y < SAMPLE_SCREEN_HEIGHT; y++) {
      for (x = 0; x < SAMPLE_SCREEN_WIDTH; x++) {
        if (s_map[y][x] == '=') {
          TCOD_console_put_char(sample_console, x, y, TCOD_CHAR_DHLINE, TCOD_BKGND_NONE);
        }
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
  for (y = 0; y < SAMPLE_SCREEN_HEIGHT; y++) {
    for (x = 0; x < SAMPLE_SCREEN_WIDTH; x++) {
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

/* ***************************
 * path sample
 * ***************************/
void render_path(bool first, TCOD_key_t* key, TCOD_mouse_t* mouse) {
  // clang-format off
  static const char* s_map[] = {
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
  static int px = 20, py = 10;  // player position
  static int dx = 24, dy = 1;   // destination
  static TCOD_map_t map = NULL;
  static TCOD_color_t dark_wall = {0, 0, 100};
  static TCOD_color_t dark_ground = {50, 50, 150};
  static TCOD_color_t light_ground = {200, 180, 50};
  static TCOD_path_t path = NULL;
  static bool usingAstar = true;
  static float dijkstraDist = 0;
  static TCOD_dijkstra_t dijkstra = NULL;
  static bool recalculatePath = false;
  static float busy;
  static int oldChar = ' ';
  int mx, my, x, y, i;
  if (!map) {
    /* initialize the map */
    map = TCOD_map_new(SAMPLE_SCREEN_WIDTH, SAMPLE_SCREEN_HEIGHT);
    for (y = 0; y < SAMPLE_SCREEN_HEIGHT; y++) {
      for (x = 0; x < SAMPLE_SCREEN_WIDTH; x++) {
        if (s_map[y][x] == ' ')
          TCOD_map_set_properties(map, x, y, true, true); /* ground */
        else if (s_map[y][x] == '=')
          TCOD_map_set_properties(map, x, y, true, false); /* window */
      }
    }
    path = TCOD_path_new_using_map(map, 1.41f);
    dijkstra = TCOD_dijkstra_new(map, 1.41f);
  }
  if (first) {
    /* we draw the foreground only the first time.
       during the player movement, only the @ is redrawn.
       the rest impacts only the background color */
    /* draw the help text & player @ */
    TCOD_console_clear(sample_console);
    TCOD_console_set_default_foreground(sample_console, TCOD_white);
    TCOD_console_put_char(sample_console, dx, dy, '+', TCOD_BKGND_NONE);
    TCOD_console_put_char(sample_console, px, py, '@', TCOD_BKGND_NONE);
    TCOD_console_printf(sample_console, 1, 1, "IJKL / mouse :\nmove destination\nTAB : A*/dijkstra");
    TCOD_console_printf(sample_console, 1, 4, "Using : A*");
    /* draw windows */
    for (y = 0; y < SAMPLE_SCREEN_HEIGHT; y++) {
      for (x = 0; x < SAMPLE_SCREEN_WIDTH; x++) {
        if (s_map[y][x] == '=') {
          TCOD_console_put_char(sample_console, x, y, TCOD_CHAR_DHLINE, TCOD_BKGND_NONE);
        }
      }
    }

    recalculatePath = true;
  }
  if (recalculatePath) {
    if (usingAstar) {
      TCOD_path_compute(path, px, py, dx, dy);
    } else {
      int x, y;
      dijkstraDist = 0.0f;
      /* compute the distance grid */
      TCOD_dijkstra_compute(dijkstra, px, py);
      /* get the maximum distance (needed for ground shading only) */
      for (y = 0; y < SAMPLE_SCREEN_HEIGHT; y++) {
        for (x = 0; x < SAMPLE_SCREEN_WIDTH; x++) {
          float d = TCOD_dijkstra_get_distance(dijkstra, x, y);
          if (d > dijkstraDist) dijkstraDist = d;
        }
      }
      // compute the path
      TCOD_dijkstra_path_set(dijkstra, dx, dy);
    }
    recalculatePath = false;
    busy = 0.2f;
  }
  // draw the dungeon
  for (y = 0; y < SAMPLE_SCREEN_HEIGHT; y++) {
    for (x = 0; x < SAMPLE_SCREEN_WIDTH; x++) {
      bool wall = s_map[y][x] == '#';
      TCOD_console_set_char_background(sample_console, x, y, wall ? dark_wall : dark_ground, TCOD_BKGND_SET);
    }
  }
  // draw the path
  if (usingAstar) {
    for (i = 0; i < TCOD_path_size(path); i++) {
      int x, y;
      TCOD_path_get(path, i, &x, &y);
      TCOD_console_set_char_background(sample_console, x, y, light_ground, TCOD_BKGND_SET);
    }
  } else {
    int x, y, i;
    for (y = 0; y < SAMPLE_SCREEN_HEIGHT; y++) {
      for (x = 0; x < SAMPLE_SCREEN_WIDTH; x++) {
        bool wall = s_map[y][x] == '#';
        if (!wall) {
          float d = TCOD_dijkstra_get_distance(dijkstra, x, y);
          TCOD_console_set_char_background(
              sample_console,
              x,
              y,
              TCOD_color_lerp(light_ground, dark_ground, 0.9f * d / dijkstraDist),
              TCOD_BKGND_SET);
        }
      }
    }
    for (i = 0; i < TCOD_dijkstra_size(dijkstra); i++) {
      int x, y;
      TCOD_dijkstra_get(dijkstra, i, &x, &y);
      TCOD_console_set_char_background(sample_console, x, y, light_ground, TCOD_BKGND_SET);
    }
  }
  // move the creature
  busy -= TCOD_sys_get_last_frame_length();
  if (busy <= 0.0f) {
    busy = 0.2f;
    if (usingAstar) {
      if (!TCOD_path_is_empty(path)) {
        TCOD_console_put_char(sample_console, px, py, ' ', TCOD_BKGND_NONE);
        TCOD_path_walk(path, &px, &py, true);
        TCOD_console_put_char(sample_console, px, py, '@', TCOD_BKGND_NONE);
      }
    } else {
      if (!TCOD_dijkstra_is_empty(dijkstra)) {
        TCOD_console_put_char(sample_console, px, py, ' ', TCOD_BKGND_NONE);
        TCOD_dijkstra_path_walk(dijkstra, &px, &py);
        TCOD_console_put_char(sample_console, px, py, '@', TCOD_BKGND_NONE);
        recalculatePath = true;
      }
    }
  }
  if (key->vk == TCODK_TAB) {
    usingAstar = !usingAstar;
    if (usingAstar)
      TCOD_console_printf(sample_console, 1, 4, "Using : A*      ");
    else
      TCOD_console_printf(sample_console, 1, 4, "Using : Dijkstra");
    recalculatePath = true;
  } else if (key->vk == TCODK_TEXT && key->text[0] != '\0') {
    if ((key->text[0] == 'I' || key->text[0] == 'i') && dy > 0) {
      // destination move north
      TCOD_console_put_char(sample_console, dx, dy, oldChar, TCOD_BKGND_NONE);
      dy--;
      oldChar = TCOD_console_get_char(sample_console, dx, dy);
      TCOD_console_put_char(sample_console, dx, dy, '+', TCOD_BKGND_NONE);
      if (s_map[dy][dx] == ' ') {
        recalculatePath = true;
      }
    } else if ((key->text[0] == 'K' || key->text[0] == 'k') && dy < SAMPLE_SCREEN_HEIGHT - 1) {
      // destination move south
      TCOD_console_put_char(sample_console, dx, dy, oldChar, TCOD_BKGND_NONE);
      dy++;
      oldChar = TCOD_console_get_char(sample_console, dx, dy);
      TCOD_console_put_char(sample_console, dx, dy, '+', TCOD_BKGND_NONE);
      if (s_map[dy][dx] == ' ') {
        recalculatePath = true;
      }
    } else if ((key->text[0] == 'J' || key->text[0] == 'j') && dx > 0) {
      // destination move west
      TCOD_console_put_char(sample_console, dx, dy, oldChar, TCOD_BKGND_NONE);
      dx--;
      oldChar = TCOD_console_get_char(sample_console, dx, dy);
      TCOD_console_put_char(sample_console, dx, dy, '+', TCOD_BKGND_NONE);
      if (s_map[dy][dx] == ' ') {
        recalculatePath = true;
      }
    } else if ((key->text[0] == 'L' || key->text[0] == 'l') && dx < SAMPLE_SCREEN_WIDTH - 1) {
      // destination move east
      TCOD_console_put_char(sample_console, dx, dy, oldChar, TCOD_BKGND_NONE);
      dx++;
      oldChar = TCOD_console_get_char(sample_console, dx, dy);
      TCOD_console_put_char(sample_console, dx, dy, '+', TCOD_BKGND_NONE);
      if (s_map[dy][dx] == ' ') {
        recalculatePath = true;
      }
    }
  }
  mx = mouse->cx - SAMPLE_SCREEN_X;
  my = mouse->cy - SAMPLE_SCREEN_Y;
  if (mx >= 0 && mx < SAMPLE_SCREEN_WIDTH && my >= 0 && my < SAMPLE_SCREEN_HEIGHT && (dx != mx || dy != my)) {
    TCOD_console_put_char(sample_console, dx, dy, oldChar, TCOD_BKGND_NONE);
    dx = mx;
    dy = my;
    oldChar = TCOD_console_get_char(sample_console, dx, dy);
    TCOD_console_put_char(sample_console, dx, dy, '+', TCOD_BKGND_NONE);
    if (s_map[dy][dx] == ' ') {
      recalculatePath = true;
    }
  }
}

/* ***************************
 * SDL callback sample
 * ***************************/
#ifndef NO_SDL_SAMPLE
TCOD_noise_t noise = NULL;
bool sdl_callback_enabled = false;
int effectNum = 0;
float delay = 3.0f;

void burn(SDL_Surface* screen, int sample_x, int sample_y, int sample_w, int sample_h) {
  int r_idx = screen->format->Rshift / 8;
  int g_idx = screen->format->Gshift / 8;
  int b_idx = screen->format->Bshift / 8;
  int x, y;
  for (x = sample_x; x < sample_x + sample_w; x++) {
    uint8_t* p = (uint8_t*)screen->pixels + x * screen->format->BytesPerPixel + sample_y * screen->pitch;
    for (y = sample_y; y < sample_y + sample_h; y++) {
      int ir = 0, ig = 0, ib = 0;
      uint8_t* p2 = p + screen->format->BytesPerPixel;  // get pixel at x+1,y
      ir += p2[r_idx];
      ig += p2[g_idx];
      ib += p2[b_idx];
      p2 -= 2 * screen->format->BytesPerPixel;  // get pixel at x-1,y
      ir += p2[r_idx];
      ig += p2[g_idx];
      ib += p2[b_idx];
      p2 += screen->format->BytesPerPixel + screen->pitch;  // get pixel at x,y+1
      ir += p2[r_idx];
      ig += p2[g_idx];
      ib += p2[b_idx];
      p2 -= 2 * screen->pitch;  // get pixel at x,y-1
      ir += p2[r_idx];
      ig += p2[g_idx];
      ib += p2[b_idx];
      ir /= 4;
      ig /= 4;
      ib /= 4;
      p[r_idx] = ir;
      p[g_idx] = ig;
      p[b_idx] = ib;
      p += screen->pitch;
    }
  }
}

void explode(SDL_Surface* screen, int sample_x, int sample_y, int sample_w, int sample_h) {
  int r_idx = screen->format->Rshift / 8;
  int g_idx = screen->format->Gshift / 8;
  int b_idx = screen->format->Bshift / 8;
  int dist = (int)(10 * (3.0f - delay));
  int x, y;
  for (x = sample_x; x < sample_x + sample_w; x++) {
    uint8_t* p = (uint8_t*)screen->pixels + x * screen->format->BytesPerPixel + sample_y * screen->pitch;
    for (y = sample_y; y < sample_y + sample_h; y++) {
      int ir = 0, ig = 0, ib = 0, i;
      for (i = 0; i < 3; i++) {
        int dx = TCOD_random_get_int(NULL, -dist, dist);
        int dy = TCOD_random_get_int(NULL, -dist, dist);
        uint8_t* p2;
        p2 = p + dx * screen->format->BytesPerPixel;
        p2 += dy * screen->pitch;
        ir += p2[r_idx];
        ig += p2[g_idx];
        ib += p2[b_idx];
      }
      ir /= 3;
      ig /= 3;
      ib /= 3;
      p[r_idx] = ir;
      p[g_idx] = ig;
      p[b_idx] = ib;
      p += screen->pitch;
    }
  }
}

void blur(SDL_Surface* screen, int sample_x, int sample_y, int sample_w, int sample_h) {
  // let's blur that sample console
  float f[3], n = 0.0f;
  int r_idx = screen->format->Rshift / 8;
  int g_idx = screen->format->Gshift / 8;
  int b_idx = screen->format->Bshift / 8;
  int x, y;
  f[2] = TCOD_sys_elapsed_seconds();
  if (noise == NULL) noise = TCOD_noise_new(3, TCOD_NOISE_DEFAULT_HURST, TCOD_NOISE_DEFAULT_LACUNARITY, NULL);
  for (x = sample_x; x < sample_x + sample_w; x++) {
    uint8_t* p = (uint8_t*)screen->pixels + x * screen->format->BytesPerPixel + sample_y * screen->pitch;
    f[0] = (float)(x) / sample_w;
    for (y = sample_y; y < sample_y + sample_h; y++) {
      int ir = 0, ig = 0, ib = 0, dec, count;
      if ((y - sample_y) % 8 == 0) {
        f[1] = (float)(y) / sample_h;
        n = TCOD_noise_get_fbm(noise, f, 3.0f);
      }
      dec = (int)(3 * (n + 1.0f));
      count = 0;
      switch (dec) {
        case 4:
          count += 4;
          // get pixel at x,y
          ir += p[r_idx];
          ig += p[g_idx];
          ib += p[b_idx];
          p -= 2 * screen->format->BytesPerPixel;  // get pixel at x+2,y
          ir += p[r_idx];
          ig += p[g_idx];
          ib += p[b_idx];
          p -= 2 * screen->pitch;  // get pixel at x+2,y+2
          ir += p[r_idx];
          ig += p[g_idx];
          ib += p[b_idx];
          p += 2 * screen->format->BytesPerPixel;  // get pixel at x,y+2
          ir += p[r_idx];
          ig += p[g_idx];
          ib += p[b_idx];
          p += 2 * screen->pitch;
        case 3:
          count += 4;
          // get pixel at x,y
          ir += p[r_idx];
          ig += p[g_idx];
          ib += p[b_idx];
          p += 2 * screen->format->BytesPerPixel;  // get pixel at x+2,y
          ir += p[r_idx];
          ig += p[g_idx];
          ib += p[b_idx];
          p += 2 * screen->pitch;  // get pixel at x+2,y+2
          ir += p[r_idx];
          ig += p[g_idx];
          ib += p[b_idx];
          p -= 2 * screen->format->BytesPerPixel;  // get pixel at x,y+2
          ir += p[r_idx];
          ig += p[g_idx];
          ib += p[b_idx];
          p -= 2 * screen->pitch;
        case 2:
          count += 4;
          // get pixel at x,y
          ir += p[r_idx];
          ig += p[g_idx];
          ib += p[b_idx];
          p -= screen->format->BytesPerPixel;  // get pixel at x-1,y
          ir += p[r_idx];
          ig += p[g_idx];
          ib += p[b_idx];
          p -= screen->pitch;  // get pixel at x-1,y-1
          ir += p[r_idx];
          ig += p[g_idx];
          ib += p[b_idx];
          p += screen->format->BytesPerPixel;  // get pixel at x,y-1
          ir += p[r_idx];
          ig += p[g_idx];
          ib += p[b_idx];
          p += screen->pitch;
        case 1:
          count += 4;
          // get pixel at x,y
          ir += p[r_idx];
          ig += p[g_idx];
          ib += p[b_idx];
          p += screen->format->BytesPerPixel;  // get pixel at x+1,y
          ir += p[r_idx];
          ig += p[g_idx];
          ib += p[b_idx];
          p += screen->pitch;  // get pixel at x+1,y+1
          ir += p[r_idx];
          ig += p[g_idx];
          ib += p[b_idx];
          p -= screen->format->BytesPerPixel;  // get pixel at x,y+1
          ir += p[r_idx];
          ig += p[g_idx];
          ib += p[b_idx];
          p -= screen->pitch;
          ir /= count;
          ig /= count;
          ib /= count;
          p[r_idx] = ir;
          p[g_idx] = ig;
          p[b_idx] = ib;
          break;
        default:
          break;
      }
      p += screen->pitch;
    }
  }
}

void SDL_render(void* sdlSurface) {
  SDL_Surface* screen = (SDL_Surface*)sdlSurface;
  // now we have almighty access to the screen's precious pixels !!
  // get the font character size
  int char_w, char_h, sample_x, sample_y;
  TCOD_sys_get_char_size(&char_w, &char_h);
  // compute the sample console position in pixels
  sample_x = SAMPLE_SCREEN_X * char_w;
  sample_y = SAMPLE_SCREEN_Y * char_h;
  delay -= TCOD_sys_get_last_frame_length();
  if (delay < 0.0f) {
    delay = 3.0f;
    effectNum = (effectNum + 1) % 3;
    if (effectNum == 2)
      sdl_callback_enabled = false;  // no forced redraw for burn effect
    else
      sdl_callback_enabled = true;
  }
  switch (effectNum) {
    case 0:
      blur(screen, sample_x, sample_y, SAMPLE_SCREEN_WIDTH * char_w, SAMPLE_SCREEN_HEIGHT * char_h);
      break;
    case 1:
      explode(screen, sample_x, sample_y, SAMPLE_SCREEN_WIDTH * char_w, SAMPLE_SCREEN_HEIGHT * char_h);
      break;
    case 2:
      burn(screen, sample_x, sample_y, SAMPLE_SCREEN_WIDTH * char_w, SAMPLE_SCREEN_HEIGHT * char_h);
      break;
  }
}

void render_sdl(bool first, TCOD_key_t* key, TCOD_mouse_t* mouse) {
  if (first) {
    // use noise sample as background. rendering is done in SampleRenderer
    TCOD_console_set_default_background(sample_console, TCOD_light_blue);
    TCOD_console_set_default_foreground(sample_console, TCOD_white);
    TCOD_console_clear(sample_console);
    TCOD_console_printf_rect_ex(
        sample_console,
        SAMPLE_SCREEN_WIDTH / 2,
        3,
        SAMPLE_SCREEN_WIDTH,
        0,
        TCOD_BKGND_NONE,
        TCOD_CENTER,
        "The SDL callback gives you access to the screen surface so that you can alter the pixels one by one using SDL "
        "API or any API on top of SDL. SDL is used here to blur the sample console.\n\nHit TAB to enable/disable the "
        "callback. While enabled, it will be active on other samples too.\n\nNote that the SDL callback only works "
        "with SDL renderer.");
  }
  if (key->vk == TCODK_TAB) {
    sdl_callback_enabled = !sdl_callback_enabled;
    if (sdl_callback_enabled) {
      TCOD_sys_register_SDL_renderer((void (*)())SDL_render);
    } else {
      TCOD_sys_register_SDL_renderer(NULL);
    }
  }
}

#endif

/* ***************************
 * the list of samples
 * ***************************/
sample_t samples[] = {
    {"  True colors        ", render_colors},
    {"  Field of view      ", render_fov},
    {"  Path finding       ", render_path},
#ifndef NO_SDL_SAMPLE
    {"  SDL callback       ", render_sdl},
#endif
};
int nb_samples = sizeof(samples) / sizeof(sample_t); /* total number of samples */

/* ***************************
 * the main function
 * ***************************/
int main(int argc, char* argv[]) {
  int cur_sample = 0; /* index of the current sample */
  bool first = true;  /* first time we render a sample */
  int i;
  TCOD_key_t key = {TCODK_NONE, 0};
  TCOD_mouse_t mouse;
  char* font = "data/fonts/dejavu10x10_gs_tc.png";
  int nb_char_horiz = 0, nb_char_vertic = 0;
  int fullscreen_width = 0;
  int fullscreen_height = 0;
  int font_flags = TCOD_FONT_TYPE_GREYSCALE | TCOD_FONT_LAYOUT_TCOD;
  int font_new_flags = 0;
  TCOD_renderer_t renderer = TCOD_RENDERER_OPENGL2; // TCOD_RENDERER_SDL2
  bool fullscreen = false;
  bool credits_end = false;
  int cur_renderer = 0;

  if (font_flags == 0)
	font_flags = font_new_flags;

  SDL_LogSetAllPriority(SDL_LOG_PRIORITY_WARN);

  TCOD_console_set_custom_font(font, font_flags, nb_char_horiz, nb_char_vertic);

  if (fullscreen_width > 0) {
    TCOD_sys_force_fullscreen_resolution(fullscreen_width, fullscreen_height);
  }

  TCOD_console_init_root(80, 50, "libtcod C sample", fullscreen, renderer);
  atexit(TCOD_quit);

  /* initialize the offscreen console for the samples */
  sample_console = TCOD_console_new(SAMPLE_SCREEN_WIDTH, SAMPLE_SCREEN_HEIGHT);
  TCOD_sys_set_fps(60);
  TCOD_sys_set_renderer(renderer);

  do {
    if (!credits_end) {
      credits_end = TCOD_console_credits_render(56, 43, false);
    }

    /* print the list of samples */
    for (i = 0; i < nb_samples; i++) {
      if (i == cur_sample) {
        /* set colors for currently selected sample */
        TCOD_console_set_default_foreground(NULL, TCOD_white);
        TCOD_console_set_default_background(NULL, TCOD_light_blue);
      } else {
        /* set colors for other samples */
        TCOD_console_set_default_foreground(NULL, TCOD_grey);
        TCOD_console_set_default_background(NULL, TCOD_black);
      }
      /* print the sample name */
      TCOD_console_printf_ex(NULL, 2, 46 - (nb_samples - i), TCOD_BKGND_SET, TCOD_LEFT, "%s", samples[i].name);
    }

    /* print the help message */
    TCOD_console_set_default_foreground(NULL, TCOD_grey);
    TCOD_console_printf_ex(NULL, 79, 46, TCOD_BKGND_NONE, TCOD_RIGHT, "last frame : %3d ms (%3d fps)", (int)(TCOD_sys_get_last_frame_length() * 1000), TCOD_sys_get_fps());
    TCOD_console_printf_ex(NULL, 79, 47, TCOD_BKGND_NONE, TCOD_RIGHT, "elapsed : %8dms %4.2fs", TCOD_sys_elapsed_milli(), TCOD_sys_elapsed_seconds());
    TCOD_console_printf(NULL, 2, 47, "%c%c : select a sample", TCOD_CHAR_ARROW_N, TCOD_CHAR_ARROW_S);
    TCOD_console_printf(NULL, 2, 48, "ALT-ENTER : switch to %s", TCOD_console_is_fullscreen() ? "windowed mode  " : "fullscreen mode");

    /* render current sample */
    samples[cur_sample].render(first, &key, &mouse);
    first = false;

    /* blit the sample console on the root console */
    TCOD_console_blit(
        sample_console,
        0,
        0,
        SAMPLE_SCREEN_WIDTH,
        SAMPLE_SCREEN_HEIGHT, /* the source console & zone to blit */
        NULL,
        SAMPLE_SCREEN_X,
        SAMPLE_SCREEN_Y, /* the destination console & position */
        1.0f,
        1.0f /* alpha coefficients */
    );

    cur_renderer = TCOD_sys_get_renderer();
    for (i = 0; i < TCOD_NB_RENDERERS; i++) {
      if (i == cur_renderer) {
        /* set colors for current renderer */
        TCOD_console_set_default_foreground(NULL, TCOD_white);
        TCOD_console_set_default_background(NULL, TCOD_light_blue);
      } else {
        /* set colors for other renderer */
        TCOD_console_set_default_foreground(NULL, TCOD_grey);
        TCOD_console_set_default_background(NULL, TCOD_black);
      }
    }

    /* update the game screen */
    TCOD_console_flush();

    /* did the user hit a key ? */
    TCOD_sys_check_for_event((TCOD_event_t)(TCOD_EVENT_KEY_PRESS | TCOD_EVENT_MOUSE), &key, &mouse);

    if (key.vk == TCODK_DOWN) {
      /* down arrow : next sample */
      cur_sample = (cur_sample + 1) % nb_samples;
      first = true;
    } else if (key.vk == TCODK_UP) {
      /* up arrow : previous sample */
      cur_sample--;
      if (cur_sample < 0) cur_sample = nb_samples - 1;
      first = true;
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
