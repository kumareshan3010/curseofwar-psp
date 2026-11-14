/******************************************************************************
  Curse of War -- PSP front end (SDL2).
  Game logic and original artwork: Copyright (C) 2013 Alexey Nikolaev (GPLv3).
  This file is part of a GPLv3 port; see LICENSE.
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL.h"

#include "common.h"
#include "grid.h"
#include "state.h"
#include "main-common.h"
#include "output-common.h"
#include "output-sdl.h"

#define SW 480
#define SH 272
#define HUD_H 32
#define TIME_DELAY 10

enum { B_TRI = 0, B_CIR = 1, B_CRO = 2, B_SQU = 3, B_L = 4, B_R = 5,
       B_DOWN = 6, B_LEFT = 7, B_UP = 8, B_RIGHT = 9, B_SEL = 10, B_START = 11 };
enum screen_id { SCR_MENU, SCR_GAME, SCR_PAUSE, SCR_END };

/* ---------- globals ---------- */
static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static SDL_Surface *scr, *big, *tileset, *typeface, *uisurf;
static SDL_Joystick *joy;
static char basedir[256] = "";

static struct state st;
static struct ui ui;
static int tile_variant[MAX_WIDTH][MAX_HEIGHT];
static int pop_variant[MAX_WIDTH][MAX_HEIGHT];
static int map_h, cam_x, cam_y, k;
static unsigned int last_seed;
static int result;
static int running = 1;
static enum screen_id screen = SCR_MENU;

/* ---------- settings (same options as the original command line) ---------- */
static const char *dif_names[] = {"Easiest", "Easy", "Normal", "Hard", "Hardest"};
static const enum config_dif dif_vals[] = {dif_easiest, dif_easy, dif_normal, dif_hard, dif_hardest};
static const char *spd_names[] = {"Slowest", "Slower", "Slow", "Normal", "Fast", "Faster", "Fastest"};
static const enum config_speed spd_vals[] = {sp_slowest, sp_slower, sp_slow, sp_normal, sp_fast, sp_faster, sp_fastest};
static const char *shape_names[] = {"Rectangle", "Rhombus", "Hexagon"};
static const enum stencil shape_vals[] = {st_rect, st_rhombus, st_hex};
static const char *preset_names[] = {"Custom", "Beginner", "Beginner, small map"};
static const char *row_names[] = {"Preset", "Difficulty", "Game speed", "Map shape", "Map width",
                                  "Map height", "Countries", "Inequality", "Your start",
                                  "Random everything", "START GAME"};
#define ROWS 11

static struct { int preset, dif, speed, shape, w, h, countries, ineq, start, rnd; }
  cfg = {0, 2, 3, 0, 21, 21, 0, -1, 0, 0};

static int cfg_max_countries(void) { return stencil_avlbl_loc_num(shape_vals[cfg.shape]); }
static int cfg_countries(void) {
  int m = cfg_max_countries();
  return (cfg.countries > 0 && cfg.countries <= m) ? cfg.countries : m;
}
static void cfg_clamp(void) {
  int m = cfg_max_countries();
  if (cfg.countries > m) cfg.countries = m;
  if (cfg.start > cfg_countries()) cfg.start = cfg_countries();
}
static int wrap(int v, int lo, int hi) { if (v < lo) return hi; if (v > hi) return lo; return v; }

static void menu_change(int row, int d) {
  switch (row) {
    case 0:
      cfg.preset = wrap(cfg.preset + d, 0, 2);
      if (cfg.preset >= 1) { cfg.dif = 0; cfg.ineq = 4; cfg.start = 1; cfg.rnd = 0; }
      if (cfg.preset == 2) { cfg.w = 16; cfg.h = 16; }
      cfg_clamp();
      return;
    case 1: cfg.dif = wrap(cfg.dif + d, 0, 4); break;
    case 2: cfg.speed = wrap(cfg.speed + d, 0, 6); break;
    case 3: cfg.shape = wrap(cfg.shape + d, 0, 2); break;
    case 4: cfg.w = wrap(cfg.w + d, 14, 30); break;
    case 5: cfg.h = wrap(cfg.h + d, 14, 29); break;
    case 6: cfg.countries = wrap(cfg_countries() + d, 2, cfg_max_countries()); break;
    case 7: cfg.ineq = wrap(cfg.ineq + d, -1, 4); break;
    case 8: cfg.start = wrap(cfg.start + d, 0, cfg_countries()); break;
    case 9: cfg.rnd = !cfg.rnd; break;
    default: return;
  }
  cfg.preset = 0;
  cfg_clamp();
}

static void menu_value(int row, char *b) {
  switch (row) {
    case 0: strcpy(b, preset_names[cfg.preset]); break;
    case 1: strcpy(b, dif_names[cfg.dif]); break;
    case 2: strcpy(b, spd_names[cfg.speed]); break;
    case 3: strcpy(b, shape_names[cfg.shape]); break;
    case 4: sprintf(b, "%d", cfg.w); break;
    case 5: sprintf(b, "%d", cfg.h); break;
    case 6: sprintf(b, "%d", cfg_countries()); break;
    case 7: if (cfg.ineq < 0) strcpy(b, "Random"); else sprintf(b, "%d", cfg.ineq); break;
    case 8:
      if (cfg.start == 0) strcpy(b, "Random");
      else if (cfg.start == 1) strcpy(b, "1 (best)");
      else if (cfg.start == cfg_countries()) sprintf(b, "%d (worst)", cfg.start);
      else sprintf(b, "%d", cfg.start);
      break;
    case 9: strcpy(b, cfg.rnd ? "On" : "Off"); break;
    default: b[0] = '\0';
  }
}

/* ---------- gamepad ---------- */
static int cur[16], prv[16];
static Uint32 rep_t[16];

static void pad_update(void) {
  int i, ax, ay;
  SDL_JoystickUpdate();
  memcpy(prv, cur, sizeof cur);
  for (i = 0; i < 14; i++) cur[i] = joy ? SDL_JoystickGetButton(joy, i) : 0;
  ax = joy ? SDL_JoystickGetAxis(joy, 0) : 0;
  ay = joy ? SDL_JoystickGetAxis(joy, 1) : 0;
  if (ay < -18000) cur[B_UP] = 1;
  if (ay > 18000) cur[B_DOWN] = 1;
  if (ax < -18000) cur[B_LEFT] = 1;
  if (ax > 18000) cur[B_RIGHT] = 1;
}
static int pressed(int b) { return cur[b] && !prv[b]; }
static int repeat(int b) {
  Uint32 now = SDL_GetTicks();
  if (!cur[b]) { rep_t[b] = 0; return 0; }
  if (!prv[b]) { rep_t[b] = now + 250; return 1; }
  if (now >= rep_t[b]) { rep_t[b] = now + 60; return 1; }
  return 0;
}

/* ---------- drawing helpers ---------- */
static void text(SDL_Surface *dst, const char *s, int x, int y) {
  for (; *s; ++s, x += TYPE_WIDTH)
    if (*s != ' ') output_char(typeface, dst, *s, x, y);
}
static void text_col(SDL_Surface *dst, int player, const char *s, int x, int y) {
  for (; *s; ++s, x += TYPE_WIDTH)
    if (*s != ' ') output_char(typeface, dst, *s + TYPE_LINE_LENGTH * (3 + player), x, y);
}
static void text_center(SDL_Surface *dst, const char *s, int y, int player) {
  int x = (SW - (int)strlen(s) * TYPE_WIDTH) / 2;
  if (player >= 0) text_col(dst, player, s, x, y); else text(dst, s, x, y);
}

/* alpha-blended rectangle on the 32-bit ABGR screen surface */
static void fill_rgba(SDL_Surface *s, int x, int y, int w, int h, int r, int g, int b, int a) {
  int xx, yy, x1 = x + w, y1 = y + h;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x1 > s->w) x1 = s->w;
  if (y1 > s->h) y1 = s->h;
  for (yy = y; yy < y1; yy++) {
    Uint32 *p = (Uint32 *)((Uint8 *)s->pixels + yy * s->pitch) + x;
    for (xx = x; xx < x1; xx++, p++) {
      Uint32 d = *p;
      int dr = d & 255, dg = (d >> 8) & 255, db = (d >> 16) & 255;
      dr += (r - dr) * a / 255;
      dg += (g - dg) * a / 255;
      db += (b - db) * a / 255;
      *p = 0xFF000000u | ((Uint32)db << 16) | ((Uint32)dg << 8) | (Uint32)dr;
    }
  }
}

static void draw_bg(void) {
  int y;
  for (y = 0; y < SH; y++) {
    SDL_Rect r = {0, y, SW, 1};
    int t = y * 255 / SH;
    SDL_FillRect(scr, &r, SDL_MapRGB(scr->format, 16 + t / 14, 20 + t / 11, 38 + t / 8));
  }
}

static void present(void) {
  SDL_UpdateTexture(tex, NULL, scr->pixels, scr->pitch);
  SDL_RenderClear(ren);
  SDL_RenderCopy(ren, tex, NULL, NULL);
  SDL_RenderPresent(ren);
}

/* ---------- menus ---------- */
static void draw_menu(int sel) {
  int i, y0 = 34, rh = 19;
  char v[48], line[64];
  draw_bg();
  text_center(scr, "CURSE OF WAR", 6, 1);
  text_center(scr, "PSP edition", 20, -1);
  fill_rgba(scr, 36, y0 - 4, 408, ROWS * rh + 6, 255, 255, 255, 20);
  for (i = 0; i < ROWS; i++) {
    int y = y0 + i * rh;
    if (i == sel) fill_rgba(scr, 40, y - 2, 400, rh - 1, 70, 150, 255, 100);
    if (i == ROWS - 1) {
      text_center(scr, row_names[i], y + 1, i == sel ? 1 : -1);
      continue;
    }
    text(scr, row_names[i], 54, y + 1);
    menu_value(i, v);
    if (i == sel) sprintf(line, "< %s >", v); else strcpy(line, v);
    if (i == sel) text_col(scr, 1, line, 428 - (int)strlen(line) * TYPE_WIDTH, y + 1);
    else text(scr, line, 428 - (int)strlen(line) * TYPE_WIDTH, y + 1);
  }
  text_center(scr, "D-pad: choose / change    X: start game", SH - 17, -1);
}

static void draw_list(const char **items, int n, int sel, int y0) {
  int i, w = 220;
  fill_rgba(scr, (SW - w) / 2 - 10, y0 - 8, w + 20, n * 22 + 12, 12, 16, 30, 225);
  for (i = 0; i < n; i++) {
    int y = y0 + i * 22;
    if (i == sel) fill_rgba(scr, (SW - w) / 2, y - 2, w, 20, 70, 150, 255, 120);
    text_center(scr, items[i], y + 2, i == sel ? 1 : -1);
  }
}

/* ---------- game ---------- */
static void make_options(struct basic_options *op, struct multi_options *mop, int same_map) {
  int avl;
  memset(op, 0, sizeof *op);
  memset(mop, 0, sizeof *mop);
  op->keep_random_flag = cfg.rnd;
  op->dif = dif_vals[cfg.dif];
  op->speed = spd_vals[cfg.speed];
  op->w = cfg.w;
  op->h = cfg.h;
  op->shape = shape_vals[cfg.shape];
  avl = stencil_avlbl_loc_num(op->shape);
  op->loc_num = cfg_countries();
  if (op->loc_num < 2) op->loc_num = 2;
  if (op->loc_num > avl) op->loc_num = avl;
  op->conditions = cfg.start;
  if (op->conditions > op->loc_num) op->conditions = op->loc_num;
  op->timeline_flag = 0;
  op->inequality = (cfg.ineq < 0) ? RANDOM_INEQUALITY : cfg.ineq;
  op->map_seed = same_map ? last_seed : (unsigned int)rand();
  mop->clients_num = 1;
  if (op->shape == st_rect) {
    int w2 = op->w + (op->h + 1) / 2;
    op->w = (w2 < MAX_WIDTH - 1) ? w2 : MAX_WIDTH - 1;
  }
}

static void start_game(int same_map) {
  struct basic_options op;
  struct multi_options mop;
  int i, j, bw, bh;
  if (!same_map) srand(SDL_GetTicks());
  make_options(&op, &mop, same_map);
  last_seed = op.map_seed;
  state_init(&st, &op, &mop);
  ui_init(&st, &ui);
  for (i = 0; i < MAX_WIDTH; ++i)
    for (j = 0; j < MAX_HEIGHT; ++j) { tile_variant[i][j] = rand(); pop_variant[i][j] = rand(); }
  bw = (ui.xlength + 2) * TILE_WIDTH;
  bh = (st.grid.height + 3) * TILE_HEIGHT + TYPE_HEIGHT * 5;
  if (bw < TILE_WIDTH + 75 * TYPE_WIDTH + TILE_WIDTH) bw = TILE_WIDTH + 75 * TYPE_WIDTH + TILE_WIDTH;
  map_h = (st.grid.height + 3) * TILE_HEIGHT;
  if (big) SDL_FreeSurface(big);
  big = SDL_CreateRGBSurfaceWithFormat(0, bw, bh, 32, SDL_PIXELFORMAT_ABGR8888);
  cam_x = ((ui.cursor.i - ui.xskip) * TILE_WIDTH + (ui.cursor.j + 1) * (TILE_WIDTH / 2)) - SW / 2;
  cam_y = (ui.cursor.j + 1) * TILE_HEIGHT - (SH - HUD_H) / 2;
  k = 0;
  result = 0;
  screen = SCR_GAME;
}

static int clampi(int v, int lo, int hi) { if (hi < lo) hi = lo; return v < lo ? lo : (v > hi ? hi : v); }

static void draw_hud(void) {
  char b[64];
  int p, y1 = SH - HUD_H + 1, y2 = SH - HUD_H + 16, y, m, d;
  fill_rgba(scr, 0, SH - HUD_H, SW, HUD_H, 8, 10, 18, 215);
  text(scr, "Gold", 6, y1);
  sprintf(b, "%li", st.country[st.controlled].gold);
  text_col(scr, st.controlled, b, 6 + 5 * TYPE_WIDTH, y1);
  time_to_ymd(st.time, &y, &m, &d);
  sprintf(b, "%i-%02i-%02i", y, m, d);
  text(scr, "Date", 150, y1);
  text_col(scr, st.controlled, b, 150 + 5 * TYPE_WIDTH, y1);
  text(scr, "Speed", 300, y1);
  {
    static const char *nm[] = {"Pause", "Slowest", "Slower", "Slow", "Normal", "Fast", "Faster", "Fastest"};
    text(scr, nm[st.speed], 300 + 6 * TYPE_WIDTH, y1);
  }
  text(scr, "Pop", 6, y2);
  for (p = 1; p < MAX_PLAYER; ++p) {
    sprintf(b, "%3i", st.grid.tiles[ui.cursor.i][ui.cursor.j].units[p][citizen]);
    text_col(scr, p, b, 6 + 4 * TYPE_WIDTH + (p - 1) * 4 * TYPE_WIDTH, y2);
  }
  text(scr, "Build 160/240/320", 300, y2);
}

static void render_game(void) {
  SDL_Rect strip, src;
  int cpx, cpy, tx, ty;
  output_sdl(tileset, typeface, uisurf, big, &st, &ui, tile_variant, pop_variant, k);
  strip.x = 0; strip.y = map_h; strip.w = big->w; strip.h = big->h - map_h;
  SDL_FillRect(big, &strip, SDL_MapRGB(big->format, 20, 20, 20));

  cpx = (ui.cursor.i - ui.xskip) * TILE_WIDTH + (ui.cursor.j + 1) * (TILE_WIDTH / 2) + TILE_WIDTH / 2;
  cpy = (ui.cursor.j + 1) * TILE_HEIGHT + TILE_HEIGHT / 2;
  tx = clampi(cpx - SW / 2, 0, big->w - SW);
  ty = clampi(cpy - (SH - HUD_H) / 2, 0, map_h - (SH - HUD_H));
  cam_x += (tx - cam_x + (tx > cam_x ? 2 : -2)) / 3;
  cam_y += (ty - cam_y + (ty > cam_y ? 2 : -2)) / 3;
  cam_x = clampi(cam_x, 0, big->w - SW);
  cam_y = clampi(cam_y, 0, map_h - (SH - HUD_H));

  SDL_FillRect(scr, NULL, SDL_MapRGB(scr->format, 0, 0, 0));
  src.x = cam_x; src.y = cam_y; src.w = SW; src.h = SH;
  SDL_BlitSurface(big, &src, scr, NULL);
  draw_hud();
}

static void game_input(void) {
  char c;
  if (repeat(B_UP)) { c = 'k'; singleplayer_process_input(&st, &ui, c); }
  if (repeat(B_DOWN)) { c = 'j'; singleplayer_process_input(&st, &ui, c); }
  if (repeat(B_LEFT)) { c = 'h'; singleplayer_process_input(&st, &ui, c); }
  if (repeat(B_RIGHT)) { c = 'l'; singleplayer_process_input(&st, &ui, c); }
  if (pressed(B_CRO)) singleplayer_process_input(&st, &ui, ' ');  /* toggle flag */
  if (pressed(B_CIR)) singleplayer_process_input(&st, &ui, 'r');  /* build / upgrade */
  if (pressed(B_SQU)) singleplayer_process_input(&st, &ui, 'c');  /* remove half of flags */
  if (pressed(B_TRI)) singleplayer_process_input(&st, &ui, 'x');  /* remove all flags */
  if (pressed(B_L)) singleplayer_process_input(&st, &ui, 's');    /* slower */
  if (pressed(B_R)) singleplayer_process_input(&st, &ui, 'f');    /* faster */
  if (pressed(B_SEL)) singleplayer_process_input(&st, &ui, 'p');  /* pause */
}

static SDL_Surface *load_asset(const char *name) {
  char path[320];
  SDL_Surface *s = NULL;
  snprintf(path, sizeof path, "images/%s", name);
  if (load_image(path, &s) == 0) return s;
  snprintf(path, sizeof path, "%simages/%s", basedir, name);
  if (load_image(path, &s) == 0) return s;
  return NULL;
}

int main(int argc, char *argv[]) {
  Uint32 prev_t, last_draw = 0;
  int sel = 0, psel = 0, n;
  static const char *pause_items[] = {"Resume", "Restart this map", "New game settings", "Quit"};
  static const char *end_items[] = {"Play again (same map)", "New game settings", "Quit"};

  if (argc > 0 && argv[0]) {
    char *slash;
    strncpy(basedir, argv[0], sizeof basedir - 1);
    slash = strrchr(basedir, '/');
    if (slash) slash[1] = '\0'; else basedir[0] = '\0';
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) return 1;
  win = SDL_CreateWindow("Curse of War", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SW, SH, 0);
  ren = SDL_CreateRenderer(win, -1, 0);
  tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, SW, SH);
  scr = SDL_CreateRGBSurfaceWithFormat(0, SW, SH, 32, SDL_PIXELFORMAT_ABGR8888);
  if (SDL_NumJoysticks() > 0) joy = SDL_JoystickOpen(0);

  tileset = load_asset("tileset.bmp");
  typeface = load_asset("type.bmp");
  uisurf = load_asset("ui.bmp");
  if (!tileset || !typeface || !uisurf) {
    Uint32 t0 = SDL_GetTicks();
    SDL_FillRect(scr, NULL, SDL_MapRGB(scr->format, 90, 20, 20));
    while (SDL_GetTicks() - t0 < 8000) {
      SDL_Event e;
      while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) t0 = 0;
      present();
      SDL_Delay(30);
    }
    SDL_Quit();
    return 1;
  }

  srand(SDL_GetTicks());
  prev_t = SDL_GetTicks();

  while (running) {
    SDL_Event e;
    Uint32 now;
    while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) running = 0;
    pad_update();
    now = SDL_GetTicks();

    switch (screen) {
      case SCR_MENU:
        if (repeat(B_UP)) sel = wrap(sel - 1, 0, ROWS - 1);
        if (repeat(B_DOWN)) sel = wrap(sel + 1, 0, ROWS - 1);
        if (repeat(B_LEFT)) menu_change(sel, -1);
        if (repeat(B_RIGHT)) menu_change(sel, 1);
        if (pressed(B_CRO) || pressed(B_START)) { start_game(0); prev_t = SDL_GetTicks(); break; }
        draw_menu(sel);
        break;

      case SCR_GAME:
        game_input();
        if (pressed(B_START)) { screen = SCR_PAUSE; psel = 0; }
        n = 0;
        while (now - prev_t >= TIME_DELAY && n < 6) {
          int slow;
          prev_t += TIME_DELAY;
          k++;
          if (k >= 1600) k = 0;
          slow = game_slowdown(st.speed);
          if (k % slow == 0 && st.speed != sp_pause) { kings_move(&st); simulate(&st); }
          if (k % 50 == 0) {
            int r = win_or_lose(&st);
            if (r != 0) { result = r; screen = SCR_END; psel = 0; }
          }
          n++;
        }
        if (now - prev_t >= TIME_DELAY * 6) prev_t = now;
        if (now - last_draw >= 30 || screen != SCR_GAME) { render_game(); last_draw = now; }
        break;

      case SCR_PAUSE:
        if (repeat(B_UP)) psel = wrap(psel - 1, 0, 3);
        if (repeat(B_DOWN)) psel = wrap(psel + 1, 0, 3);
        if (pressed(B_START) || pressed(B_CIR)) { screen = SCR_GAME; prev_t = now; break; }
        if (pressed(B_CRO)) {
          if (psel == 0) { screen = SCR_GAME; prev_t = now; }
          else if (psel == 1) { start_game(1); prev_t = SDL_GetTicks(); }
          else if (psel == 2) screen = SCR_MENU;
          else running = 0;
          break;
        }
        render_game();
        fill_rgba(scr, 0, 0, SW, SH, 0, 0, 0, 120);
        text_center(scr, "PAUSED", 40, 1);
        draw_list(pause_items, 4, psel, 70);
        text_center(scr, "X flag  O build  [] half flags  /\\ all flags", SH - 52, -1);
        text_center(scr, "L slower  R faster  Select pause  Start menu", SH - 36, -1);
        break;

      case SCR_END:
        if (repeat(B_UP)) psel = wrap(psel - 1, 0, 2);
        if (repeat(B_DOWN)) psel = wrap(psel + 1, 0, 2);
        if (pressed(B_CRO)) {
          if (psel == 0) { start_game(1); prev_t = SDL_GetTicks(); }
          else if (psel == 1) screen = SCR_MENU;
          else running = 0;
          break;
        }
        render_game();
        fill_rgba(scr, 0, 0, SW, SH, 0, 0, 0, 140);
        text_center(scr, result > 0 ? "VICTORY" : "DEFEAT", 50, result > 0 ? 1 : 2);
        draw_list(end_items, 3, psel, 90);
        break;
    }
    present();
    SDL_Delay(1);
  }

  SDL_Quit();
  return 0;
}
