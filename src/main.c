/* main.c — WinMain (21f0) + wndproc (1bc9) equivalent for SDL2.
 *
 * Message mapping:
 *   GetMessage loop        -> SDL_WaitEventTimeout (1000ms -> WM_TIMER -> 2fe0)
 *   TranslateAccelerator   -> F2 -> 510, F1 -> 590
 *   WM_KEYDOWN             -> handle_keydown (SHIFT/F4/F5/F6 + XYZZY matcher)
 *   WM_LBUTTONDOWN/UP      -> drag start / commit
 *   WM_RBUTTONDOWN         -> flag or chord assist
 *   WM_MBUTTONDOWN         -> chord drag
 *   WM_MOUSEMOVE           -> drag cursor or XYZZY probe
 *   WM_ACTIVATE(2)         -> FOCUS_LOST suppress
 *   WM_SYSCOMMAND          -> MINIMIZE pause / RESTORE resume
 *   WM_WINDOWPOSCHANGED    -> save pos unless minimizing
 *   WM_DESTROY / Exit      -> quit; save config if dirty
 */
#include "winmine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

int g_quit = 0;

static const char XYZZY[5] = { 'X', 'Y', 'Z', 'Z', 'Y' };  /* DAT_01005034 */

static Uint32 timer_deadline;

static void reset_timer_deadline(void)
{
    timer_deadline = SDL_GetTicks() + 1000;
}

/* 3ab0 first-run defaults == 2b27 load defaults */
static void set_defaults(void)
{
    memset(&G, 0, sizeof G);
    G.width = 9;  G.height = 9;
    G.pend_width = 9; G.pend_height = 9; G.pend_mines = 10;
    G.difficulty = 0;
    G.marks = 1;
    G.sound = 0;
    G.color = 1;
    G.menu_visible = 0;
    G.win_x = 0x50; G.win_y = 0x50;
    for (int i = 0; i < 3; i++) {
        G.best_time[i] = 999;
        snprintf(G.best_name[i], sizeof(G.best_name[i]), "Anonymous");
    }
}

static void resolve_asset_dir(char *out, size_t n)
{
    char exe[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (len > 0) {
        exe[len] = 0;
        char *slash = strrchr(exe, '/');
        if (slash) {
            snprintf(out, n, "%.*s/assets", 400, exe);
            if (access(out, 0) == 0) return;
        }
    }
    snprintf(out, n, "assets");
}

/* MK_* key-state mask for the low word of lParam */
static int mk_mask(void)
{
    int dummy;
    Uint8 st = (Uint8)SDL_GetMouseState(&dummy, &dummy);
    int mk = 0;
    if (st & SDL_BUTTON_LMASK) mk |= 1;                 /* MK_LBUTTON */
    if (st & SDL_BUTTON_RMASK) mk |= 2;                 /* MK_RBUTTON */
    Uint32 mods = SDL_GetModState();
    if (mods & KMOD_SHIFT) mk |= 4;                     /* MK_SHIFT   */
    if (mods & KMOD_CTRL)  mk |= 8;                     /* MK_CONTROL */
    return mk;
}

/* start a press-drag: SetCapture + cursor=(-1,-1) + surprised face */
static void drag_begin(void)
{
    SDL_CaptureMouse(SDL_TRUE);
    G.cursor_x = -1;
    G.cursor_y = -1;
    G.dragging = 1;
    draw_face(1);
    render_present();
}

static void drag_end_no_commit(void)
{
    G.dragging = 0;
    SDL_CaptureMouse(SDL_FALSE);
    G.cursor_x = -2;
    G.cursor_y = -2;
    gm_move_cursor(-2, -2);
}

/* WM_KEYDOWN (0x100) */
static void handle_keydown(SDL_Event *e)
{
    SDL_Keycode k = e->key.keysym.sym;
    DBG("KEYDOWN k=%d\n", (int)k);

    switch (k) {
    case SDLK_F2:  ui_command(510); return;   /* accelerator table */
    case SDLK_F1:  ui_command(590); return;   /* accelerator table */
    case SDLK_F4:                              /* 0x73 sound cycle */
        DBG("F4 sound=%d\n", G.sound);
        if (G.sound > 1) {
            if (G.sound == 3) {
                sound_stop();                 /* 38d7 */
                G.sound = 2;
            } else {
                G.sound = 2 + sound_init();   /* 38c2 */
            }
        }
        return;
    case SDLK_F5:                              /* 0x74: hide menu */
        if (G.menu_visible != 0) ui_menu_refresh(1);
        return;
    case SDLK_F6:                              /* 0x75: show menu */
        if (G.menu_visible != 0) ui_menu_refresh(2);
        return;
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:                          /* 0x10: XYZZY stage 5 -> 17 */
        if (G.xyzzycnt > 4)
            G.xyzzycnt ^= 0x14;
        return;
    default: break;
    }

    /* XYZZY matcher: cnt<5 -> cnt = (word[cnt] != vk) ? 0 : cnt+1.
     * VK for letters is uppercase; SDL letter syms are lowercase. */
    int vk = 0;
    if (k >= SDLK_a && k <= SDLK_z)      vk = 'A' + (int)(k - SDLK_a);
    else if (k >= SDLK_0 && k <= SDLK_9) vk = '0' + (int)(k - SDLK_0);
    if (G.xyzzycnt < 5)
        G.xyzzycnt = (XYZZY[G.xyzzycnt] != vk) ? 0 : G.xyzzycnt + 1;
}

/* WM_MOUSEMOVE probe branch (drag == 0) */
static void handle_probe(int mx, int cy, int mk)
{
    if (G.xyzzycnt == 0) return;

    int fire;
    if (G.xyzzycnt == 5)
        fire = (mk & 8) != 0;      /* stage 5 fires only with MK_CONTROL */
    else
        fire = 1;                  /* stages 1..4 and 17 always fire */
    if (!fire) return;

    G.cursor_x = (mx + 4) >> 4;
    G.cursor_y = (cy - 0x27) >> 4;
    if (G.cursor_x != 0 && 0 < G.cursor_y &&
        G.cursor_x <= G.width && G.cursor_y <= G.height) {
        G.probe_active = 1;
        G.probe_mine = (G.grid[G.cursor_x + G.cursor_y * GRID_COLS] & F_MINE) != 0;
        draw_probe_dot();
        render_present();
    }
}

static void handle_mouse_button(SDL_Event *e, int is_down, int button)
{
    int mx = e->button.x, my = e->button.y;
    int mk = mk_mask();
    int mo = render_menu_off();
    int cx = (mx + 4) >> 4;
    int cy = (my - mo - 0x27) >> 4;

    if (is_down) {
        DBG("BUTTONDOWN btn=%d at (%d,%d) mk=%d suppress=%d flags=%x drag=%d\n",
            button, mx, my, mk, G.suppress_click, G.flags, G.dragging);
        if (button == SDL_BUTTON_LEFT) {
            /* WM_LBUTTONDOWN */
            if (G.suppress_click) { G.suppress_click = 0; return; }
            if (mo > 0 && my < mo) {              /* menu bar (non-client) */
                int top = ui_bar_hit(mx, my);
                DBG("BAR mx=%d my=%d mo=%d top=%d\n", mx, my, mo, top);
                if (top >= 0) {
                    int cmd = ui_menu_open(top);
                    DBG("MENU top=%d cmd=%d\n", top, cmd);
                    if (cmd) {
                        paint_all();              /* menu dismissed before command */
                        render_present();
                        ui_command(cmd);
                    }
                    reset_timer_deadline();
                }
                return;
            }
            if (ui_smiley_click(mx, my - mo)) {   /* 140c */
                reset_timer_deadline();
                return;
            }
            if (!(G.flags & FL_STARTED)) return;
            G.chord = (mk & 6) != 0;              /* MK_RBUTTON | MK_SHIFT */
            drag_begin();
            gm_move_cursor(cx, cy);
            return;
        }
        if (button == SDL_BUTTON_RIGHT) {
            /* WM_RBUTTONDOWN */
            if (G.suppress_click) { G.suppress_click = 0; return; }
            if (mo > 0 && my < mo) {              /* right-click opens menu */
                int top = ui_bar_hit(mx, my);
                if (top >= 0) {
                    int cmd = ui_menu_open(top);
                    if (cmd) {
                        paint_all();              /* menu dismissed before command */
                        render_present();
                        ui_command(cmd);
                    }
                    reset_timer_deadline();
                }
                return;
            }
            if (!(G.flags & FL_STARTED)) return;
            if (G.dragging) {
                gm_move_cursor(-3, -3);
                G.chord = 1;
                gm_move_cursor(cx, cy);           /* PostMessage(0x200) */
                return;
            }
            if (!(mk & 1)) {
                if (G.menu_loop == 0)
                    gm_flag(cx, cy);              /* 374f */
                return;
            }
            drag_begin();                         /* fall through: chord press */
            gm_move_cursor(cx, cy);               /* shared tail: 31d4(cell) */
            return;
        }
        if (button == SDL_BUTTON_MIDDLE) {
            /* WM_MBUTTONDOWN */
            if (G.suppress_click) { G.suppress_click = 0; return; }
            if (!(G.flags & FL_STARTED)) return;
            G.chord = 1;
            drag_begin();
            gm_move_cursor(cx, cy);               /* shared tail: 31d4(cell) */
            return;
        }
        return;
    }

    /* WM_LBUTTONUP / RBUTTONUP / MBUTTONUP */
    DBG("BUTTONUP btn=%d at (%d,%d) drag=%d cursor=(%d,%d) flags=%x\n",
        button, mx, my, G.dragging, G.cursor_x, G.cursor_y, G.flags);
    if (!G.dragging) return;
    if (G.flags & FL_STARTED) {
        G.dragging = 0;
        SDL_CaptureMouse(SDL_FALSE);
        gm_commit();                              /* 37e1 */
    } else {
        drag_end_no_commit();
    }
}

static void handle_mouse_motion(SDL_Event *e)
{
    int mx = e->motion.x, my = e->motion.y;
    int mo = render_menu_off();
    int cy = my - mo;

    if (G.dragging) {
        if (!(G.flags & FL_STARTED)) {
            drag_end_no_commit();
        } else {
            gm_move_cursor((mx + 4) >> 4, (cy - 0x27) >> 4);
        }
        return;
    }
    handle_probe(mx, cy, mk_mask());
}

static void handle_window_event(SDL_Event *e)
{
    DBG("WINDOWEVENT %d\n", e->window.event);
    switch (e->window.event) {
    case SDL_WINDOWEVENT_CLOSE:
        g_quit = 1;
        break;
    case SDL_WINDOWEVENT_MINIMIZED:              /* SC_MINIMIZE */
        gm_pause();
        G.flags |= FL_PAUSED | FL_MINIMIZING;   /* |= 0xa */
        break;
    case SDL_WINDOWEVENT_RESTORED:               /* SC_RESTORE */
        G.flags &= 0xf5;
        gm_resume();
        G.suppress_click = 0;
        paint_all();
        ui_draw_menu(-1);
        render_present();
        break;
    case SDL_WINDOWEVENT_FOCUS_GAINED:           /* WM_ACTIVATE wParam 1/3 */
        DBG("FOCUS_GAINED\n");
        G.focused = 1;
        break;
    case SDL_WINDOWEVENT_FOCUS_LOST:             /* WM_ACTIVATE wParam==2 */
        DBG("FOCUS_LOST focused=%d -> suppress\n", G.focused);
        if (G.focused)
            G.suppress_click = 1;
        G.focused = 0;
        break;
    case SDL_WINDOWEVENT_MOVED:                  /* WM_WINDOWPOSCHANGED */
        if (!(G.flags & FL_MINIMIZING)) {
            G.win_x = e->window.data1;
            G.win_y = e->window.data2;
        }
        break;
    default:
        break;
    }
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    g_debug = getenv("WINMINE_DEBUG") != NULL;

    srand((unsigned)time(NULL));      /* original: srand(GetTickCount()) */

    set_defaults();                   /* 3ab0 */
    config_load();                    /* 2bc2 */

    char adir[512];
    resolve_asset_dir(adir, sizeof(adir));
    if (render_init(adir) != 0) {     /* 2b14: 2414 + 2ed5 */
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Minesweeper",
                                 "Cannot load game assets.", NULL);
        SDL_Quit();
        return 1;
    }
    if (G.sound == 3)
        G.sound = 2 + sound_init();   /* 2bc2 tail: 38c2 re-init */
    if (render_init_window() != 0) {  /* CreateWindowExW */
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Minesweeper",
                                 "Cannot create window.", NULL);
        SDL_Quit();
        return 1;
    }

    ui_init();
    gm_clear_grid();                  /* 2ed5 before first paint */
    ui_menu_refresh(G.menu_visible);  /* 3ce5 */
    gm_new_game();                    /* 367a */
    SDL_ShowWindow(render_window());  /* ShowWindow */

    reset_timer_deadline();           /* SetTimer(1, 1000ms) */

    SDL_Event e;
    while (!g_quit) {
        Sint32 wait = (Sint32)(timer_deadline - SDL_GetTicks());
        if (wait < 0) wait = 0;
        if (!SDL_WaitEventTimeout(&e, wait)) {
            gm_timer_tick();          /* WM_TIMER -> 2fe0 */
            reset_timer_deadline();
            render_present();         /* original draws straight to window DC */
            continue;
        }
        switch (e.type) {
        case SDL_QUIT:
            g_quit = 1;
            break;
        case SDL_WINDOWEVENT:
            handle_window_event(&e);
            break;
        case SDL_KEYDOWN:
            handle_keydown(&e);
            break;
        case SDL_MOUSEBUTTONDOWN:
            handle_mouse_button(&e, 1, e.button.button);
            break;
        case SDL_MOUSEBUTTONUP:
            handle_mouse_button(&e, 0, e.button.button);
            break;
        case SDL_MOUSEMOTION:
            handle_mouse_motion(&e);
            break;
        default:
            break;
        }
        /* original GDI blits reach the window immediately; mirror that by
         * presenting after every event the handlers may have drawn for */
        render_present();
    }

    /* exit: 263c cleanup, then 2dab if DAT_0100515c != 0 */
    DBG("EXIT sound=%d color=%d marks=%d dirty=%d\n", G.sound, G.color, G.marks, G.dirty);
    sound_exit();
    if (G.dirty)
        config_save();
    ui_free();
    render_free_assets();
    SDL_Quit();
    return 0;
}
