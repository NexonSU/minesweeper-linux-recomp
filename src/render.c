/* render.c — SDL2 rendering faithful to WINMINE.EXE draw functions:
 * 2414 (pen), 2607 (palette), 2646 (cell blit), 26a7 (field), 2752 (digits),
 * 2785 (counter), 2825 (timer), 28d9 (face), 293d/2971 (staircase frames),
 * 2a22 (frames), 2ac3 (full paint), 1950 (resize), XYZZY probe dot.
 *
 * Asset row mapping (verified by pixel-shape analysis of the extracted sheets):
 *   cells_color.png  16 rows x 16px: row = 15 - tile  (row0 covered, row15 pressed-blank)
 *   digits_color.png 12 rows x 13px: row = 11 - chunk (row0 minus, row1 blank, rows2..11 = 9..0)
 *   faces_color.png   5 rows x 24px: row = 4 - face
 *
 * The Win95 menu bar was non-client area; here it is the top MENU_H pixels of the
 * window surface, so all client-area drawing is offset by render_menu_off().
 */
#include "winmine.h"
#include <SDL2/SDL.h>
#include <stdio.h>

static SDL_Window *g_window = NULL;
static SDL_Surface *g_screen = NULL;   /* whole window surface (menu + client) */

static SDL_Surface *g_cells = NULL, *g_digits = NULL, *g_faces = NULL;

static int g_pen_color = 0x808080;     /* DAT_01005158 frame pen (2414) */

/* ---------- menu offset (menu shown iff (menu_visible & 1) == 0) ---------- */
int render_menu_off(void)
{
    return (G.menu_visible & 1) == 0 ? MENU_H : 0;
}

/* ---------- client geometry (1950: DAT_01005b2c / DAT_01005b20) ---------- */
int render_client_w(void) { return G.width * 0x10 + 0x18; }
int render_client_h(void) { return G.height * 0x10 + 0x43; }

/* ---------- 2414: frame pen color ---------- */
static void update_pen(void)
{
    g_pen_color = G.color ? 0x808080 : 0x000000;
}

/* ---------- sheet loading ---------- */
static SDL_Surface *load_sheet(const char *dir, const char *name)
{
    char path[600];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    SDL_Surface *s = stb_load_surface(path);
    if (!s) {
        fprintf(stderr, "load_sheet %s: image decode failed\n", path);
        return NULL;
    }
    return s;
}

static char g_asset_dir[512];

const char *render_asset_dir(void) { return g_asset_dir; }

int render_init(const char *asset_dir)
{
    snprintf(g_asset_dir, sizeof(g_asset_dir), "%s", asset_dir);
    if (render_reload_assets() != 0)        return -1;
    return 0;
}

/* 2607 + 2414: reload bitmaps and pen for the current color mode */
int render_reload_assets(void)
{
    SDL_FreeSurface(g_cells);
    SDL_FreeSurface(g_digits);
    SDL_FreeSurface(g_faces);
    g_cells  = load_sheet(g_asset_dir, G.color ? "cells_color.png"  : "cells_mono.png");
    g_digits = load_sheet(g_asset_dir, G.color ? "digits_color.png" : "digits_mono.png");
    g_faces  = load_sheet(g_asset_dir, G.color ? "faces_color.png"  : "faces_mono.png");
    update_pen();
    return (g_cells && g_digits && g_faces) ? 0 : -1;
}

void render_free_assets(void)
{
    SDL_FreeSurface(g_cells);  g_cells  = NULL;
    SDL_FreeSurface(g_digits); g_digits = NULL;
    SDL_FreeSurface(g_faces);  g_faces  = NULL;
}

/* blit row-indexed sheet (row counted from top), 1:1 opaque like SRCCOPY */
static void blit_sheet(SDL_Surface *sheet, int row, int row_h, int x, int y)
{
    if (!sheet) return;
    SDL_Rect src = { 0, row * row_h, row_h, row_h };
    SDL_Rect dst = { x, y + render_menu_off(), row_h, row_h };
    if (src.y < 0 || src.y + src.h > sheet->h) return;
    SDL_BlitSurface(sheet, &src, g_screen, &dst);
}

/* ---------- GDI MoveTo/LineTo emulation (LineTo excludes endpoint) ---------- */
static int g_pen_x = 0, g_pen_y = 0;

static void pen_move(int x, int y) { g_pen_x = x; g_pen_y = y; }

static void pen_line(int x, int y, Uint32 col)
{
    int x0 = g_pen_x, y0 = g_pen_y;
    if (!g_screen) return;
    Uint32 *px = (Uint32 *)g_screen->pixels;
    int pitch = g_screen->pitch / 4;
    if (x0 == x) {                       /* vertical */
        int lo = y0, hi = y;
        if (lo > hi) { int t = lo; lo = hi; hi = t; }
        for (int yy = lo; yy < hi; yy++)             /* endpoint excluded */
            if (yy >= 0 && yy < g_screen->h && x0 >= 0 && x0 < g_screen->w)
                px[yy * pitch + x0] = col;
    } else if (y0 == y) {                /* horizontal */
        int lo = x0, hi = x;
        if (lo > hi) { int t = lo; lo = hi; hi = t; }
        for (int xx = lo; xx < hi; xx++)
            if (xx >= 0 && xx < g_screen->w && y0 >= 0 && y0 < g_screen->h)
                px[y0 * pitch + xx] = col;
    }
    g_pen_x = x; g_pen_y = y;
}

/* ---------- 2971: staircase frame (293d pen semantics) ----------
 * style&1==0 -> R2_COPYPEN with frame pen; style&1==1 -> R2_WHITE.
 * style<2 -> pass 2 uses style^1 (opposite color); style>=2 -> same both passes. */
void stair_frame(int x1, int y1, int x2, int y2, int depth, int style)
{
    int mo = render_menu_off();
    Uint32 c1 = (style & 1) ? 0x00FFFFFFu : (Uint32)g_pen_color;
    Uint32 c2 = (style < 2) ? ((style ^ 1) & 1 ? 0x00FFFFFFu : (Uint32)g_pen_color) : c1;

    int cx1 = x1, cy1 = y1 + mo, cx2 = x2, cy2 = y2 + mo;

    /* pass 1 (top-left staircase), repeated depth times */
    for (int i = 0; i < depth; i++) {
        cy2--;
        pen_move(cx1, cy2);
        pen_line(cx1, cy1, c1);
        cx1++;
        pen_line(cx2, cy1, c1);
        cx2--;
        cy1++;
    }
    /* pass 2 (bottom-right staircase), repeated depth times */
    for (int i = 0; i < depth; i++) {
        cy2++;
        pen_move(cx1, cy2);
        cx1--;
        cx2++;
        pen_line(cx2, cy2, c2);
        cy1--;
        pen_line(cx2, cy1, c2);
    }
}

/* ---------- 2a22: draw all window frames ---------- */
void draw_frames(void)
{
    int cw = render_client_w();
    int ch = render_client_h();
    int fx = (cw - 0x18) >> 1;

    stair_frame(0, 0, cw - 1, ch - 1, 3, 1);                    /* outer, raised */
    stair_frame(9, 0x34, cw - 10, ch - 10, 3, 0);               /* field, sunken */
    stair_frame(9, 9, cw - 10, 0x2d, 2, 0);                     /* top strip, sunken */
    stair_frame(0x10, 0xf, 0x38, 0x27, 1, 0);                   /* counter */
    stair_frame((cw - 2) - 0x39, 0xf, (cw - 2) - 0x11, 0x27, 1, 0); /* timer */
    stair_frame(fx - 1, 0xf, fx + 0x18, 0x28, 1, 2);            /* face, all gray */
}

/* ---------- 2646: draw one cell ---------- */
void draw_cell(int x, int y)
{
    if (x < 1 || x > G.width || y < 1 || y > G.height) return;
    int tile = G.grid[x + y * GRID_COLS] & 0x1f;
    blit_sheet(g_cells, 15 - tile, 16, x * 16 - 4, y * 16 + 0x27);
}

/* ---------- 26a7: draw whole field ---------- */
void draw_field(void)
{
    for (int y = 1; y <= G.height; y++)
        for (int x = 1; x <= G.width; x++)
            draw_cell(x, y);
}

/* ---------- 2752: draw one 13x23 digit chunk at client x ---------- */
static void draw_digit_chunk(int chunk, int x)
{
    int row = 11 - chunk;
    if (row < 0 || row > 11) return;
    blit_sheet(g_digits, row, 23, x, 0x10);
}

/* ---------- 2785: mine counter ---------- */
void draw_counter(void)
{
    int v = G.mine_counter, hi, lo;
    if (v < 0) {
        hi = 0xb;                 /* minus chunk */
        lo = -v;
    } else {
        hi = v / 100;
        lo = v;
    }
    draw_digit_chunk(hi, 0x11);
    draw_digit_chunk((lo % 100) / 10, 0x1e);
    draw_digit_chunk(lo % 10, 0x2b);
}

/* ---------- 2825: timer ---------- */
void draw_timer(void)
{
    int v = G.timer;
    int base = (render_client_w() - 2) - 0x38;
    draw_digit_chunk(v / 100, base);
    draw_digit_chunk((v % 100) / 10, base + 0xd);
    draw_digit_chunk(v % 10, base + 0x1a);
}

/* ---------- 28d9 / 2913: face ---------- */
void draw_face(int f)
{
    int fx = (render_client_w() - 0x18) >> 1;
    blit_sheet(g_faces, 4 - f, 24, fx, 0x10);
}

/* ---------- XYZZY probe dot: 1px at window corner (0,0) ---------- */
void draw_probe_dot(void)
{
    if (!G.probe_active || !g_screen) return;
    Uint32 col = G.probe_mine ? 0x00000000u : 0x00FFFFFFu;
    /* original: GetDC(NULL) + SetPixel(hdc,0,0) = screen corner;
     * port approximation: window corner */
    ((Uint32 *)g_screen->pixels)[0] = col;
}

/* ---------- 2ac3: full repaint ---------- */
void paint_all(void)
{
    if (!g_screen) return;
    /* class hbrBackground = GetStockObject(1) = LTGRAY_BRUSH */
    SDL_FillRect(g_screen, NULL, 0x00C0C0C0u);
    draw_frames();
    draw_counter();
    draw_face(G.face);
    draw_timer();
    draw_field();
    draw_probe_dot();
    /* menu bar is non-client in the original and survives client repaints */
    if (render_menu_off() > 0)
        ui_draw_menu(-1);
}

/* ---------- window management ---------- */
int render_init_window(void)
{
    /* original: CreateWindow (not yet shown) -> 1950 -> ... -> ShowWindow.
     * Create hidden so the WM does not churn focus while we size the window. */
    int cw = render_client_w(), ch = render_client_h();
    g_window = SDL_CreateWindow("Minesweeper",
        G.win_x, G.win_y, cw, ch + render_menu_off(), SDL_WINDOW_HIDDEN);
    if (!g_window) {
        g_window = SDL_CreateWindow("Minesweeper",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            cw, ch + render_menu_off(), SDL_WINDOW_HIDDEN);
    }
    if (!g_window) return -1;
    g_screen = SDL_GetWindowSurface(g_window);
    return g_screen ? 0 : -1;
}

/* 1950: recompute size, shift window if it hangs off screen, resize/invalidate */
void render_apply_window_size(int mode)
{
    if (!g_window) return;

    int cw = render_client_w(), ch = render_client_h();
    int mh = render_menu_off();

    int x = 0, y = 0;
    SDL_GetWindowPosition(g_window, &x, &y);

    SDL_Rect bounds;
    SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(g_window), &bounds);

    /* shift left/up if the window would extend past the right/bottom edge */
    int over = (cw + x) - (bounds.x + bounds.w);
    if (over > 0) { x -= over; mode |= 2; }
    over = (ch + mh + y) - (bounds.y + bounds.h);
    if (over > 0) { y -= over; mode |= 2; }

    if (mode & 2) {
        SDL_SetWindowSize(g_window, cw, ch + mh);
        SDL_SetWindowPosition(g_window, x, y);
        G.win_x = x;
        G.win_y = y;
    }
    g_screen = SDL_GetWindowSurface(g_window);
    if (mode & 4) {
        paint_all();
        render_present();
    }
}

void render_present(void)
{
    if (g_screen && g_window)
        SDL_UpdateWindowSurface(g_window);
}

SDL_Window *render_window(void) { return g_window; }
SDL_Surface *render_screen(void) { return g_screen; }
