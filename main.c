#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#ifdef PSP
#include <pspkernel.h>
PSP_MODULE_INFO("CurseOfWar", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
#endif
#define W 15
#define H 8
#define T 32
typedef struct { int k, o, v; float n; } Tile;      /* k: 0 grass 1 forest 2 mountain 3 water */
typedef struct { int o, n, tx, ty, act; float x, y; } Unit;
static Tile M[H][W];
static Unit U[64];
static int cx = 1, cy = 4, selx = -1, sely = -1, over = 0;
static float aiT = 0;

static void init(void) {
  srand(SDL_GetTicks());
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
    int r = rand() % 100;
    M[y][x] = (Tile){ r < 12 ? 3 : r < 27 ? 1 : r < 35 ? 2 : 0, 0, 0, 0 };
  }
  int sx[2] = {1, 13}, sy[2] = {4, 3};
  for (int i = 0; i < 2; i++) {
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
      Tile *m = &M[sy[i] + dy][sx[i] + dx]; m->k = 0; m->o = i + 1; m->n = 3;
    }
    M[sy[i]][sx[i]].v = 1; M[sy[i]][sx[i]].n = 20;
  }
  for (int i = 0; i < 9; i++) {
    Tile *m = &M[rand() % H][rand() % W];
    if (m->k != 3 && !m->o) { m->v = 1; m->n = 6; }
  }
  memset(U, 0, sizeof U); cx = 1; cy = 4; selx = -1; over = 0; aiT = 0;
}

static void send(int ax, int ay, int bx, int by) {
  Tile *a = &M[ay][ax]; int n = (int)(a->n / 2);
  if (n < 1 || M[by][bx].k == 3) return;
  for (int i = 0; i < 64; i++) if (!U[i].act) {
    U[i] = (Unit){ a->o, n, bx, by, 1, ax * T + 16.f, ay * T + 16.f };
    a->n -= n; return;
  }
}

static void land(Unit *u) {
  Tile *m = &M[u->ty][u->tx];
  if (m->o == u->o) { m->n += u->n; return; }
  m->n -= u->n / (m->k == 2 ? 1.5f : 1.f);
  if (m->n < 0) { m->o = u->o; m->n = -m->n; }
}

static void ai(void) {
  int bx = -1, by = -1; float bn = 8;
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
    if (M[y][x].o == 2 && M[y][x].n > bn) { bn = M[y][x].n; bx = x; by = y; }
  if (bx < 0) return;
  int tx = -1, ty = -1; float sc = 1e9f;
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
    Tile *m = &M[y][x]; if (m->k == 3 || m->o == 2) continue;
    float dx = x - bx, dy = y - by;
    float s = m->n + SDL_sqrtf(dx * dx + dy * dy) * 1.5f - (m->v ? 6 : 0) + (m->k == 2 ? 4 : 0);
    if (s < sc) { sc = s; tx = x; ty = y; }
  }
  if (tx >= 0) send(bx, by, tx, ty);
}

static void update(float dt) {
  if (over) return;
  int p = 0, e = 0;
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
    Tile *m = &M[y][x];
    if (m->o) { m->n += (m->v ? 1.2f : .12f) * dt; if (m->n > 99) m->n = 99; }
    if (m->o == 1) p++;
    if (m->o == 2) e++;
  }
  aiT += dt; if (aiT > 1.6f) { aiT = 0; ai(); }
  int moving = 0;
  for (int i = 0; i < 64; i++) if (U[i].act) {
    Unit *u = &U[i]; moving |= (u->o == 1);
    float dx = u->tx * T + 16 - u->x, dy = u->ty * T + 16 - u->y, d = SDL_sqrtf(dx * dx + dy * dy), s = 75 * dt;
    if (d <= s) { land(u); u->act = 0; } else { u->x += dx / d * s; u->y += dy / d * s; }
  }
  if (!e) over = 1; else if (!p && !moving) over = 2;
}

static void rect(SDL_Renderer *r, int x, int y, int w, int h, int R, int G, int B, int A) {
  SDL_SetRenderDrawColor(r, R, G, B, A);
  SDL_Rect q = {x, y, w, h}; SDL_RenderFillRect(r, &q);
}

static void draw(SDL_Renderer *r) {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  rect(r, 0, 0, 480, 272, 16, 19, 26, 255);
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
    Tile *m = &M[y][x]; int px = x * T, py = y * T, ch = (x + y) & 1;
    if (m->k == 3) { rect(r, px, py, T, T, 47, 111, 168, 255); continue; }
    rect(r, px, py, T, T, ch ? 98 : 90, ch ? 150 : 140, ch ? 80 : 75, 255);
    if (m->k == 1) { rect(r, px + 6, py + 8, 8, 14, 30, 90, 50, 255); rect(r, px + 18, py + 12, 8, 14, 40, 110, 60, 255); }
    if (m->k == 2) rect(r, px + 4, py + 8, 24, 20, 140, 134, 124, 255);
    if (m->o) rect(r, px, py, T, T, m->o == 1 ? 60 : 255, m->o == 1 ? 140 : 70, m->o == 1 ? 255 : 70, 80);
    if (m->v) {
      rect(r, px + 8, py + 10, 16, 16, 217, 207, 184, 255);
      rect(r, px + 14, py + 18, 5, 8, 90, 74, 58, 255);
      rect(r, px + 22, py + 2, 8, 6, m->o == 1 ? 74 : m->o == 2 ? 255 : 220, m->o == 1 ? 168 : m->o == 2 ? 90 : 220, m->o == 1 ? 255 : m->o == 2 ? 90 : 220, 255);
    }
    int bw = (int)(m->n > 30 ? 30 : m->n); if (bw > 0) rect(r, px + 1, py + 29, bw, 2, 255, 255, 255, 230);
  }
  for (int i = 0; i < 64; i++) if (U[i].act)
    rect(r, (int)U[i].x - 4, (int)U[i].y - 4, 8, 8, U[i].o == 1 ? 74 : 255, U[i].o == 1 ? 168 : 90, U[i].o == 1 ? 255 : 90, 255);
  if (selx >= 0) { SDL_SetRenderDrawColor(r, 255, 255, 255, 255); SDL_Rect q = {selx * T + 2, sely * T + 2, T - 4, T - 4}; SDL_RenderDrawRect(r, &q); }
  SDL_SetRenderDrawColor(r, 255, 220, 60, 255); SDL_Rect c = {cx * T, cy * T, T, T}; SDL_RenderDrawRect(r, &c);
  if (over) rect(r, 0, 0, 480, 256, over == 1 ? 40 : 120, over == 1 ? 160 : 30, 40, 120);
  SDL_RenderPresent(r);
}

int main(int argc, char **argv) {
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
  SDL_Window *win = SDL_CreateWindow("Curse of War", 0, 0, 480, 272, 0);
  SDL_Renderer *ren = SDL_CreateRenderer(win, -1, 0);
  if (SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);
  init();
  Uint32 last = SDL_GetTicks(); int run = 1;
  while (run) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) run = 0;
      if (e.type == SDL_JOYBUTTONDOWN) switch (e.jbutton.button) {
        case 8: if (cy > 0) cy--; break;        /* up */
        case 6: if (cy < H - 1) cy++; break;    /* down */
        case 7: if (cx > 0) cx--; break;        /* left */
        case 9: if (cx < W - 1) cx++; break;    /* right */
        case 2:                                  /* cross */
          if (over) { init(); break; }
          if (selx < 0) { if (M[cy][cx].o == 1) { selx = cx; sely = cy; } }
          else { if (M[sely][selx].o == 1 && !(cx == selx && cy == sely)) send(selx, sely, cx, cy); selx = -1; }
          break;
        case 1: selx = -1; break;                /* circle */
        case 11: if (over) init(); break;        /* start */
      }
    }
    Uint32 now = SDL_GetTicks(); float dt = (now - last) / 1000.f; if (dt > .05f) dt = .05f; last = now;
    update(dt); draw(ren); SDL_Delay(8);
  }
  SDL_Quit();
  return 0;
}
