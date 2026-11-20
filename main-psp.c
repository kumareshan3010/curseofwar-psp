/******************************************************************************
  Curse of War - native PSP frontend.
  Game logic and original artwork: Copyright (C) 2013 Alexey Nikolaev.
  Distributed under GPL-3.0-or-later; see LICENSE.
******************************************************************************/

#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "SDL.h"
#include <pspkernel.h>
#include <pspctrl.h>

PSP_MODULE_INFO("CurseOfWar", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

#include "common.h"
#include "grid.h"
#include "state.h"
#include "main-common.h"
#include "output-sdl.h"

#define PSP_W 480
#define PSP_H 272
#define LOGICAL_W 800
#define LOGICAL_H 360
#define FRAME_MS 10
#define MOVE_REPEAT_MS 115

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *frame_texture;
static SDL_Surface *canvas;
static SDL_Surface *tileset;
static SDL_Surface *typeface;
static SDL_Surface *uisurf;
static struct state game;
static struct ui game_ui;
static int tile_variant[MAX_WIDTH][MAX_HEIGHT];
static int pop_variant[MAX_WIDTH][MAX_HEIGHT];
static unsigned int previous_buttons;
static Uint32 last_move;

static void present(void) {
  SDL_Rect viewport = {0, 28, PSP_W, 216};
  SDL_UpdateTexture(frame_texture, NULL, canvas->pixels, canvas->pitch);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, frame_texture, NULL, &viewport);
  SDL_RenderPresent(renderer);
}

static int pressed(unsigned int buttons, unsigned int mask) {
  return (buttons & mask) && !(previous_buttons & mask);
}

static char read_command(void) {
  SceCtrlData pad;
  unsigned int buttons;
  char command = '\0';
  Uint32 now = SDL_GetTicks();
  sceCtrlReadBufferPositive(&pad, 1);
  buttons = pad.Buttons;

  if (pressed(buttons, PSP_CTRL_START)) command = 'q';
  else if (pressed(buttons, PSP_CTRL_CROSS)) command = ' ';
  else if (pressed(buttons, PSP_CTRL_CIRCLE)) command = 'r';
  else if (pressed(buttons, PSP_CTRL_SQUARE)) command = 'x';
  else if (pressed(buttons, PSP_CTRL_TRIANGLE)) command = 'p';
  else if (pressed(buttons, PSP_CTRL_LTRIGGER)) command = 's';
  else if (pressed(buttons, PSP_CTRL_RTRIGGER)) command = 'f';
  else if (pressed(buttons, PSP_CTRL_SELECT)) command = 'c';

  if (command == '\0' && now - last_move >= MOVE_REPEAT_MS) {
    if ((buttons & PSP_CTRL_LEFT) || pad.Lx < 82) command = 'h';
    else if ((buttons & PSP_CTRL_RIGHT) || pad.Lx > 173) command = 'l';
    else if ((buttons & PSP_CTRL_UP) || pad.Ly < 82) command = 'k';
    else if ((buttons & PSP_CTRL_DOWN) || pad.Ly > 173) command = 'j';
    if (command != '\0') last_move = now;
  }

  previous_buttons = buttons;
  return command;
}

static int load_assets(void) {
  if (load_image("images/tileset.bmp", &tileset) != 0) return 0;
  if (load_image("images/type.bmp", &typeface) != 0) return 0;
  if (load_image("images/ui.bmp", &uisurf) != 0) return 0;
  return 1;
}

static void draw_title(void) {
  SDL_FillRect(canvas, NULL, SDL_MapRGB(canvas->format, 18, 26, 34));
  output_string(typeface, canvas, "CURSE OF WAR", 310, 112);
  output_string(typeface, canvas, "A REAL-TIME STRATEGY GAME", 265, 150);
  output_string(typeface, canvas, "PRESS CROSS TO START", 292, 220);
  output_string(typeface, canvas, "START BUTTON: QUIT", 310, 250);
  present();
}

static void init_game(void) {
  struct basic_options options;
  struct multi_options multi;
  int i, j;

  srand((unsigned int)time(NULL));
  get_options(0, NULL, &options, &multi);

  /* A compact map keeps the original game HUD visible on the PSP screen. */
  options.w = 14;
  options.h = 14;
  options.loc_num = 4;
  options.conditions = 1;
  options.inequality = 4;
  options.shape = st_rect;
  options.dif = dif_easy;
  options.speed = sp_normal;
  options.keep_random_flag = 0;
  options.timeline_flag = 0;
  options.map_seed = (unsigned int)rand();
  multi.multiplayer_flag = 0;
  multi.server_flag = 0;
  multi.clients_num = 1;

  state_init(&game, &options, &multi);
  ui_init(&game, &game_ui);

  for (i = 0; i < MAX_WIDTH; ++i) {
    for (j = 0; j < MAX_HEIGHT; ++j) {
      tile_variant[i][j] = rand();
      pop_variant[i][j] = rand();
    }
  }

  free(multi.val_client_port);
  free(multi.val_server_addr);
  free(multi.val_server_port);
}

static void run_game(void) {
  int finished = 0;
  int tick = 0;
  Uint32 previous = SDL_GetTicks();

  while (!finished) {
    Uint32 now = SDL_GetTicks();
    if (now - previous < FRAME_MS) {
      SDL_Delay(1);
      continue;
    }
    previous += FRAME_MS;
    ++tick;
    if (tick >= 1600) tick = 0;

    {
      char command = read_command();
      if (command != '\0')
        finished = singleplayer_process_input(&game, &game_ui, command);
    }

    if (!finished && game.speed != sp_pause &&
        tick % game_slowdown(game.speed) == 0) {
      kings_move(&game);
      simulate(&game);
    }

    if (tick % 5 == 0) {
      output_sdl(tileset, typeface, uisurf, canvas, &game, &game_ui,
                 tile_variant, pop_variant, tick);
      present();
    }
  }
}

int main(void) {
  int started = 0;

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
    sceKernelExitGame();
    return 1;
  }

  window = SDL_CreateWindow("Curse of War", SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, PSP_W, PSP_H, 0);
  if (!window) {
    fprintf(stderr, "SDL window failed: %s\n", SDL_GetError());
    SDL_Quit();
    sceKernelExitGame();
    return 1;
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  canvas = SDL_CreateRGBSurfaceWithFormat(0, LOGICAL_W, LOGICAL_H, 32,
                                          SDL_PIXELFORMAT_ABGR8888);
  if (!renderer || !canvas) {
    fprintf(stderr, "Unable to create renderer or canvas: %s\n", SDL_GetError());
    if (canvas) SDL_FreeSurface(canvas);
    if (renderer) SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    sceKernelExitGame();
    return 1;
  }

  frame_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    LOGICAL_W, LOGICAL_H);
  if (!frame_texture || !load_assets()) {
    fprintf(stderr, "Unable to create frame texture or load assets: %s\n",
            SDL_GetError());
    if (frame_texture) SDL_DestroyTexture(frame_texture);
    SDL_FreeSurface(canvas);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    sceKernelExitGame();
    return 1;
  }

  sceCtrlSetSamplingCycle(0);
  sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
  init_game();
  draw_title();

  while (!started) {
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);
    if (pad.Buttons & PSP_CTRL_START) break;
    if (pad.Buttons & PSP_CTRL_CROSS) {
      started = 1;
      previous_buttons = pad.Buttons;
    }
    SDL_Delay(20);
  }

  if (started) run_game();

  SDL_FreeSurface(tileset);
  SDL_FreeSurface(typeface);
  SDL_FreeSurface(uisurf);
  SDL_DestroyTexture(frame_texture);
  SDL_FreeSurface(canvas);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  sceKernelExitGame();
  return 0;
}
