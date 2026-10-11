/* winmine.h - shared state for the Linux reimplementation of WINMINE.EXE */
#ifndef WINMINE_H
#define WINMINE_H

#include <SDL2/SDL.h>
#include <stdio.h>

/* ---- grid model (mirrors DAT_01005340[32*27], 1-based playfield, 0-based border ring) ---- */
#define GRID_COLS 32
#define GRID_ROWS 27

/* cell state values (low 5 bits) */
#define ST_BLANK     0    /* revealed, no adjacent mines */
#define ST_FLAG      14   /* covered + flag */
#define ST_QUESTION  13   /* covered + question mark */
#define ST_COVERED   15   /* covered */
#define ST_BORDER    16   /* border ring, never drawn */
#define F_MINE       0x80 /* has mine */
#define F_REVEALED   0x40 /* revealed */

/* window geometry */
#define MENU_H      19    /* height of the drawn menu bar */
#define SM_CXBORDER1 2    /* DAT_01005a90 = SM_CXBORDER + 1 */

/* flags (DAT_01005000) */
#define FL_STARTED  1
#define FL_PAUSED   2
#define FL_MINIMIZING 8
#define FL_ENDED    0x10

typedef struct {
    int width, height;                 /* current playfield size (DAT_01005334/5338) */
    int pend_width, pend_height, pend_mines; /* pending size (DAT_010056ac/56a8/56a4) */
    unsigned char grid[GRID_COLS * GRID_ROWS];

    int mine_counter;                  /* DAT_01005194, may go negative */
    int timer;                         /* DAT_0100579c, caps at 999 */
    int safe_count;                    /* DAT_010057a0 */
    int revealed;                      /* DAT_010057a4 */
    int face;                          /* DAT_01005160: 0 smile 1 surprised 2 dead 3 cool 4 smile-variant */
    int chord;                         /* DAT_01005144 */
    int cursor_x, cursor_y;            /* DAT_01005118/511c; -1/-2/-3 = none */
    int dragging;                      /* DAT_01005140 */
    int suppress_click;                /* DAT_01005148 */
    int focused;                       /* WM_ACTIVATE: window currently active */
    int xyzzycnt;                      /* DAT_01005154 */
    int menu_loop;                     /* DAT_0100514c: WM_ENTERMENULOOP/EXITMENULOOP */
    int dirty;                         /* DAT_0100515c */
    int difficulty;                    /* DAT_010056a0: 0..2 preset, 3 custom */
    int marks;                         /* DAT_010056bc */
    int sound;                         /* DAT_010056b8: 3 on, 2 available-off, 0 off */
    int color;                         /* DAT_010056c8: 1 color, 0 mono */
    int menu_visible;                  /* DAT_010056c4: menu shown iff (v & 1) == 0 */
    int flags;                         /* DAT_01005000 */
    int timer_running;                 /* DAT_01005164 */
    int timer_saved;                   /* DAT_01005168 */
    int best_time[3];                  /* DAT_010056cc/56d0/56d4 */
    char best_name[3][33];             /* DAT_010056d8/5718/5758 */
    int win_x, win_y;                  /* DAT_010056b0/56b4 */
    int queue_x[100], queue_y[100];    /* DAT_010051a0/57c0 flood queue */
    int queue_idx;                     /* DAT_01005798 */
    int probe_active, probe_mine;      /* XYZZY corner dot state */
} Game;

extern Game G;

/* ---- game logic (game.c) ---- */
/* debug tracing (WINMINE_DEBUG env) */
extern int g_debug;
#define DBG(...) do { if (g_debug) fprintf(stderr, __VA_ARGS__); } while (0)

void gm_clear_grid(void);                 /* 2ed5 */
void gm_new_game(void);                   /* 367a */
int  gm_neighbors(int x, int y, int mask, int val); /* 2f3b / 3119 helper */
void gm_open_cell(int x, int y);          /* 3008 */
void gm_flood(int x, int y);              /* 3084 */
void gm_press(int x, int y);              /* 316b */
void gm_unpress(int x, int y);            /* 31a0 */
void gm_move_cursor(int nx, int ny);      /* 31d4 */
void gm_set_cell(int x, int y, int tile); /* 2eab */
void gm_reveal_all(int tile);             /* 2f80 */
void gm_endgame(int win);                 /* 347c */
void gm_click(int x, int y);              /* 3512 */
void gm_chord(int x, int y);              /* 35b7 */
void gm_flag(int x, int y);               /* 374f */
void gm_commit(void);                     /* 37e1 */
void gm_counter_add(int d);               /* 346a */
void gm_timer_tick(void);                 /* 2fe0 */
void gm_pause(void);                      /* 341c */
void gm_resume(void);                     /* 344c */

/* ---- rendering (render.c) ---- */
SDL_Surface *stb_load_surface(const char *path);   /* PNG -> ARGB8888 (stb_image) */
int  render_init(const char *asset_dir);
const char *render_asset_dir(void);
void render_free_assets(void);
int  render_reload_assets(void);          /* 2607 + 2414 (color toggle) */
int  render_client_w(void);               /* DAT_01005b2c */
int  render_client_h(void);               /* DAT_01005b20 */
int  render_init_window(void);
void render_present(void);
int  render_menu_off(void);
SDL_Window  *render_window(void);
SDL_Surface *render_screen(void);
void render_apply_window_size(int mode);  /* 1950: bit1=resize, bit2=invalidate */
void draw_cell(int x, int y);             /* 2646 */
void draw_field(void);                    /* 26a7 */
void draw_counter(void);                  /* 2785 via 2801 */
void draw_timer(void);                    /* 2825 via 28b5 */
void draw_face(int f);                    /* 28d9 */
void stair_frame(int x1, int y1, int x2, int y2, int depth, int style); /* 2971 */
void draw_frames(void);                   /* 2a22 */
void paint_all(void);                     /* 2ac3 */
void draw_probe_dot(void);

/* ---- UI: menu bar + dialogs (ui.c) ---- */
void ui_init(void);
void ui_free(void);
void ui_command(int id);                  /* WM_COMMAND dispatch */
void ui_ask_name(void);                   /* 1b81: name-entry dialog (600) */
void ui_best_times(void);                 /* 1baa: best-times dialog (700) */
void ui_menu_refresh(int v);              /* 3ce5 */
int  ui_smiley_click(int px, int py);     /* 140c modal loop; returns 1 if handled */
int  ui_menu_open(int top);               /* modal dropdown loop; returns cmd id or 0 */
int  ui_bar_hit(int px, int py);          /* top-level index 0/1 or -1 (window coords) */
void ui_draw_menu(int open_top);          /* draw bar + dropdown onto window surface */
void ui_help(int cmd);                    /* 3d76 / 3d1d */

/* ---- config (config.c) ---- */
void config_load(void);
void config_save(void);

/* ---- sound (sound.c) ---- */
int  sound_init(void);
void sound_exit(void);
void sound_play(int id);                  /* 38ed: 1 tick, 2 win, 3 boom */
void sound_stop(void);                    /* 38d7 */

/* ---- main ---- */
extern int g_quit;

#endif
