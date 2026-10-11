/* ui.c - menu bar, dropdowns and Win95-style modal dialogs for the WINMINE port.
 *
 * Menu model mirrors MENU resource 500 (see res_out.txt): every item carries a
 * mnemonic '&', "New" has accelerator F2, "Contents" F1.  Checkmarks follow
 * FUN_01001516: 521-523 = difficulty, 527 = marks, 529 = color, 526 = sound != 0.
 * Dialog control rectangles were measured from the DLU template (px = dlu*1.5 x dlu*1.625)
 * and verified against a wine capture.
 */
#include "winmine.h"
#include <SDL2/SDL_image.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

/* ---------------- fonts & colors ---------------- */

static TTF_Font *f_menu, *f_dlg;

#define COL_FACE 0xC0C0C0u
#define COL_NAVY 0x000080u
#define COL_DK   0x404040u
#define COL_MID  0x808080u
#define COL_WHT  0xFFFFFFu
#define COL_TEXT_DK 0x000000u

static const SDL_Color col_text  = {0, 0, 0, 255};
static const SDL_Color col_white = {255, 255, 255, 255};

static char g_font_path[512] = "";

void ui_init(void)
{
    static const char *paths[] = {
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    };
    const char *font = NULL;
    for (unsigned i = 0; i < sizeof(paths) / sizeof(paths[0]); i++)
        if (access(paths[i], R_OK) == 0) { font = paths[i]; break; }
    if (TTF_Init() != 0) {
        SDL_Log("TTF_Init failed: %s", SDL_GetError());
        return;
    }
    if (font) snprintf(g_font_path, sizeof(g_font_path), "%s", font);
    f_menu = font ? TTF_OpenFont(font, 13) : NULL;
    f_dlg  = font ? TTF_OpenFont(font, 12) : NULL;
    if (!f_menu || !f_dlg)
        SDL_Log("ui: could not open UI font: %s", SDL_GetError());
}

void ui_free(void)
{
    if (f_menu) { TTF_CloseFont(f_menu); f_menu = NULL; }
    if (f_dlg)  { TTF_CloseFont(f_dlg);  f_dlg  = NULL; }
    TTF_Quit();
}

/* ---------------- tiny draw helpers ---------------- */

static void px(SDL_Surface *s, int x, int y, unsigned int c)
{
    if (x < 0 || y < 0 || x >= s->w || y >= s->h) return;
    *(Uint32 *)((char *)s->pixels + (size_t)y * s->pitch + (size_t)x * 4) = c;
}

static void fill_r(SDL_Surface *s, int x, int y, int w, int h, unsigned int c)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            px(s, x + i, y + j, c);
}

static void hline(SDL_Surface *s, int x0, int x1, int y, unsigned int c)
{
    for (int x = x0; x <= x1; x++) px(s, x, y, c);
}

static void vline(SDL_Surface *s, int x, int y0, int y1, unsigned int c)
{
    for (int y = y0; y <= y1; y++) px(s, x, y, c);
}

static void bevel_raise(SDL_Surface *s, int x, int y, int w, int h)
{
    fill_r(s, x, y, w, h, COL_FACE);
    hline(s, x, x + w - 2, y, COL_WHT);
    vline(s, x, y, y + h - 2, COL_WHT);
    hline(s, x + 1, x + w - 1, y + h - 1, COL_DK);
    vline(s, x + w - 1, y + 1, y + h - 1, COL_DK);
}

static void bevel_sunken(SDL_Surface *s, int x, int y, int w, int h)
{
    fill_r(s, x, y, w, h, COL_WHT);
    hline(s, x, x + w - 1, y, COL_MID);
    vline(s, x, y, y + h - 1, COL_MID);
    hline(s, x + 1, x + w - 1, y, COL_DK);          /* inner top */
    vline(s, x, y + 1, y + h - 1, COL_DK);          /* inner left */
    hline(s, x, x + w - 1, y + h - 1, COL_WHT);
    vline(s, x + w - 1, y, y + h - 1, COL_WHT);
}

static SDL_Surface *txt_surf(const char *s, SDL_Color c, TTF_Font *f)
{
    return TTF_RenderText_Blended(f, s, c);
}

static int txt_w(const char *s, TTF_Font *f)
{
    int w = 0, h = 0;
    if (f) TTF_SizeText(f, s, &w, &h);
    return w;
}

static int txt_h(TTF_Font *f)
{
    return f ? TTF_FontLineSkip(f) : 14;
}

/* render text, shrinking the dialog font until it fits maxw (MS Sans Serif
 * 8pt is narrower than DejaVu; this keeps original dialog geometry intact) */
static SDL_Surface *txt_surf_fit(const char *s, SDL_Color c, int maxw)
{
    int w12 = txt_w(s, f_dlg);
    int size = 12;
    while (size > 8 && w12 * size / 12 > maxw)
        size--;
    if (size == 12 || !g_font_path[0])
        return txt_surf(s, c, f_dlg);
    TTF_Font *f = TTF_OpenFont(g_font_path, size);
    if (!f) return txt_surf(s, c, f_dlg);
    SDL_Surface *t = TTF_RenderText_Blended(f, s, c);
    TTF_CloseFont(f);
    return t;
}

static void blit(SDL_Surface *dst, SDL_Surface *s, int x, int y)
{
    SDL_Rect r = {x, y, 0, 0};
    SDL_BlitSurface(s, NULL, dst, &r);
    SDL_FreeSurface(s);
}

/* draw text with a mnemonic underline under s[mark] */
static void draw_text_mark(SDL_Surface *dst, const char *s, int mark,
                           int x, int y, SDL_Color c, TTF_Font *f)
{
    blit(dst, txt_surf(s, c, f), x, y);
    if (mark < 0 || !f) return;
    int len = (int)strlen(s);
    if (mark >= len) return;
    char buf[64];
    if (len > 63) return;
    memcpy(buf, s, (size_t)mark);
    buf[mark] = 0;
    int pw = txt_w(buf, f);
    char ch[2] = {s[mark], 0};
    int cw = txt_w(ch, f);
    int uy = y + TTF_FontAscent(f) + 1;
    hline(dst, x + pw, x + pw + cw - 1, uy, c.r << 16 | c.g << 8 | c.b);
}

static void draw_check(SDL_Surface *s, int x, int y, SDL_Color c)
{
    unsigned col = c.r << 16 | c.g << 8 | c.b;
    /* Win95-style check glyph, 2px thick */
    for (int i = 0; i <= 4; i++) { px(s, x + 1 + i, y + 7 + i, col); px(s, x + 2 + i, y + 7 + i, col); }
    for (int i = 0; i <= 7; i++) { px(s, x + 5 + i, y + 11 - i, col); px(s, x + 5 + i, y + 10 - i, col); }
}

static void dotted_rect(SDL_Surface *s, int x, int y, int w, int h)
{
    for (int i = 0; i < w; i++) { px(s, x + i, y, i & 1 ? COL_TEXT_DK : COL_WHT); px(s, x + i, y + h - 1, i & 1 ? COL_WHT : COL_TEXT_DK); }
    for (int i = 0; i < h; i++) { px(s, x, y + i, i & 1 ? COL_WHT : COL_TEXT_DK); px(s, x + w - 1, y + i, i & 1 ? COL_TEXT_DK : COL_WHT); }
}

static int alt_held(void)
{
    int n;
    const Uint8 *ks = SDL_GetKeyboardState(&n);
    return ks[SDL_SCANCODE_LALT] || ks[SDL_SCANCODE_RALT];
}

/* ---------------- menu model (resource 500) ---------------- */

typedef struct {
    const char *label;   /* '&' stripped */
    int id;              /* 0 = separator */
    int check;           /* 0..2 difficulty, 3 marks, 4 color, 5 sound!=0, -1 none */
    int mark;            /* mnemonic char index */
    const char *accel;
} MItem;

static const MItem game_items[] = {
    {"New",           510, -1, 0, "F2"},
    {NULL,              0, -1, -1, NULL},
    {"Beginner",      521,  0, 0, NULL},
    {"Intermediate",  522,  1, 0, NULL},
    {"Expert",        523,  2, 0, NULL},
    {"Custom...",     524, -1, 0, NULL},
    {NULL,              0, -1, -1, NULL},
    {"Marks (?)",     527,  3, 0, NULL},
    {"Color",         529,  4, 1, NULL},
    {"Sound",         526,  5, 0, NULL},
    {NULL,              0, -1, -1, NULL},
    {"Best Times...", 528, -1, 5, NULL},
    {NULL,              0, -1, -1, NULL},
    {"Exit",          512, -1, 2, NULL},
};

static const MItem help_items[] = {
    {"Contents",              590, -1, 0, "F1"},
    {"Search for Help on...", 591, -1, 0, NULL},
    {"Using Help",            592, -1, 5, NULL},
    {NULL,                      0, -1, -1, NULL},
    {"About Minesweeper...",   593, -1, 0, NULL},
};

static const char *top_labels[2] = {"Game", "Help"};
static const MItem *menus[2] = {game_items, help_items};
static const int menu_n[2] = {
    (int)(sizeof(game_items) / sizeof(game_items[0])),
    (int)(sizeof(help_items) / sizeof(help_items[0])),
};

static int item_checked(const MItem *it)
{
    switch (it->check) {
    case 0: return G.difficulty == 0;
    case 1: return G.difficulty == 1;
    case 2: return G.difficulty == 2;
    case 3: return G.marks != 0;
    case 4: return G.color != 0;
    case 5: return G.sound != 0;
    default: return 0;
    }
}

/* ---------------- menu bar + dropdown drawing ---------------- */

#define ITEM_H 19
#define SEP_H    8
#define GUTTER  21

static int top_x[2], top_w[2];

typedef struct { int x, y, w, h; } Rct;
static Rct dd_items[16];
static int dd_n, dd_top = -1, dd_x, dd_y, dd_w, dd_h;

static void layout_bar(void)
{
    int x = 4;
    for (int i = 0; i < 2; i++) {
        top_w[i] = txt_w(top_labels[i], f_menu) + 14;
        top_x[i] = x;
        x += top_w[i];
    }
}

int ui_bar_hit(int mx, int my)
{
    if (my >= MENU_H) return -1;
    for (int i = 0; i < 2; i++)
        if (mx >= top_x[i] && mx < top_x[i] + top_w[i])
            return i;
    return -1;
}

static void draw_menu_full(SDL_Surface *s, int open, int sel)
{
    layout_bar();
    fill_r(s, 0, 0, s->w, MENU_H, COL_WHT);   /* COLOR_MENU bar = white */
    int show_marks = alt_held();
    for (int i = 0; i < 2; i++) {
        SDL_Color c = col_text;
        if (open == i) {
            fill_r(s, top_x[i], 0, top_w[i], MENU_H, COL_NAVY);
            c = col_white;
        }
        int h = txt_h(f_menu);
        draw_text_mark(s, top_labels[i], show_marks ? 0 : -1,
                       top_x[i] + 7, (MENU_H - h) / 2, c, f_menu);
    }

    dd_top = -1;
    if (open < 0) return;

    const MItem *its = menus[open];
    int n = menu_n[open];
    int pw = 0, ph = 0;
    for (int i = 0; i < n; i++) {
        if (!its[i].id) { ph += SEP_H; continue; }
        int w = GUTTER + txt_w(its[i].label, f_menu) + 14;
        if (its[i].accel) w += 8 + txt_w(its[i].accel, f_menu);
        if (w > pw) pw = w;
        ph += ITEM_H;
    }
    dd_x = top_x[open];
    dd_y = MENU_H;
    dd_w = pw;
    dd_h = ph;

    fill_r(s, dd_x, dd_y, pw, ph, COL_FACE);
    hline(s, dd_x, dd_x + pw - 2, dd_y, COL_WHT);
    vline(s, dd_x, dd_y, dd_y + ph - 2, COL_WHT);
    hline(s, dd_x + 1, dd_x + pw - 1, dd_y + ph - 1, COL_DK);
    vline(s, dd_x + pw - 1, dd_y + 1, dd_y + ph - 1, COL_DK);

    int y = dd_y;
    for (int i = 0; i < n && i < 16; i++) {
        if (!its[i].id) {
            hline(s, dd_x + 2, dd_x + pw - 3, y + 3, COL_MID);
            hline(s, dd_x + 2, dd_x + pw - 3, y + 4, COL_WHT);
            dd_items[i].w = 0;
            y += SEP_H;
            continue;
        }
        dd_items[i] = (Rct){dd_x + 1, y, pw - 2, ITEM_H};
        SDL_Color c = (i == sel) ? col_white : col_text;
        if (i == sel)
            fill_r(s, dd_x + 1, y, pw - 2, ITEM_H, COL_NAVY);
        if (item_checked(&its[i]))
            draw_check(s, dd_x + 4, y + 3, c);
        int h = txt_h(f_menu);
        draw_text_mark(s, its[i].label, its[i].mark,
                       dd_x + GUTTER, y + (ITEM_H - h) / 2, c, f_menu);
        if (its[i].accel) {
            int aw = txt_w(its[i].accel, f_menu);
            blit(s, txt_surf(its[i].accel, c, f_menu), dd_x + pw - 10 - aw, y + (ITEM_H - h) / 2);
        }
        y += ITEM_H;
    }
    for (int i = n; i < 16; i++) dd_items[i].w = 0;
    dd_n = n;
    dd_top = open;
}

void ui_draw_menu(int open_top)
{
    draw_menu_full(render_screen(), open_top, -1);
}

/* dropdown item index at window coords, -1 if none */
static int dd_hit_index(int mx, int my)
{
    if (dd_top < 0) return -1;
    if (mx < dd_x || mx >= dd_x + dd_w || my < dd_y || my >= dd_y + dd_h)
        return -1;
    for (int i = 0; i < dd_n; i++) {
        Rct *r = &dd_items[i];
        if (r->w && mx >= r->x && mx < r->x + r->w && my >= r->y && my < r->y + r->h)
            return i;
    }
    return -1;
}

static int next_item(int top, int sel, int dir)
{
    const MItem *its = menus[top];
    int n = menu_n[top];
    int i = sel;
    for (int k = 0; k < n; k++) {
        i += dir;
        if (i < 0) i = 0;
        if (i >= n) i = n - 1;
        if (its[i].id) return i;
    }
    return sel;
}

/* modal dropdown loop; returns command id or 0 */
int ui_menu_open(int top)
{
    G.menu_loop = 1;
    int open = top, sel = -1, cmd = 0, done = 0;
    SDL_Window *win = render_window();
    Uint32 winid = SDL_GetWindowID(win);

    while (!done && !g_quit) {
        paint_all();
        draw_menu_full(render_screen(), open, sel);
        render_present();

        SDL_Event e;
        if (!SDL_WaitEvent(&e)) break;
        if (e.type == SDL_WINDOWEVENT && e.window.windowID != winid)
            continue;

        switch (e.type) {
        case SDL_MOUSEBUTTONDOWN:
            if (e.button.button != SDL_BUTTON_LEFT) break;
            {
                int t = ui_bar_hit(e.button.x, e.button.y);
                if (t >= 0) {
                    if (t == open) done = 1;
                    else { open = t; sel = -1; }
                } else {
                    int i = dd_hit_index(e.button.x, e.button.y);
                    if (i >= 0 && menus[open][i].id) {
                        cmd = menus[open][i].id;
                        done = 1;
                    } else {
                        done = 1;   /* click outside closes the menu */
                    }
                }
            }
            break;
        case SDL_MOUSEMOTION:
            {
                int t = ui_bar_hit(e.motion.x, e.motion.y);
                if (t >= 0) {
                    if (t != open) { open = t; sel = -1; }
                } else {
                    int i = dd_hit_index(e.motion.x, e.motion.y);
                    if (i >= 0) sel = i;
                }
            }
            break;
        case SDL_KEYDOWN:
            switch (e.key.keysym.sym) {
            case SDLK_ESCAPE: done = 1; break;
            case SDLK_LEFT:  if (open > 0) { open--; sel = -1; } break;
            case SDLK_RIGHT: if (open < 1) { open++; sel = -1; } break;
            case SDLK_DOWN:  sel = next_item(open, sel, +1); break;
            case SDLK_UP:    sel = next_item(open, sel, -1); break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if (sel >= 0 && menus[open][sel].id) {
                    cmd = menus[open][sel].id;
                    done = 1;
                }
                break;
            }
            break;
        case SDL_QUIT:
            g_quit = 1;
            done = 1;
            break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_CLOSE) { g_quit = 1; done = 1; }
            break;
        }
    }
    G.menu_loop = 0;
    dd_top = -1;
    return cmd;
}

/* ---------------- 3ce5: menu visibility refresh ---------------- */

void ui_menu_refresh(int v)
{
    G.menu_visible = v;
    render_apply_window_size(2);   /* 1950(2): resize + reposition */
    paint_all();
    ui_draw_menu(-1);
    render_present();
}

/* ---------------- 140c: smiley click modal loop ---------------- */

int ui_smiley_click(int px_, int py_)
{
    int cw = render_client_w();
    int x0 = (cw - 0x18) >> 1;
    if (!(px_ >= x0 && px_ < x0 + 0x24 && py_ >= 0x10 && py_ < 0x28))
        return 0;

    SDL_CaptureMouse(SDL_TRUE);
    int saved_face = G.face;
    int drawing = 1;
    G.face = 4;
    draw_face(4);
    render_present();

    int done = 0, up_inside = 0;
    while (!done && !g_quit) {
        SDL_Event e;
        if (!SDL_WaitEvent(&e)) break;
        switch (e.type) {
        case SDL_MOUSEMOTION:
            {
                int cx = e.motion.x;
                int cy = e.motion.y - render_menu_off();
                int inside = (cx >= x0 && cx < x0 + 0x24 && cy >= 0x10 && cy < 0x28);
                if (inside != drawing) {
                    drawing = inside;
                    G.face = inside ? 4 : saved_face;
                    draw_face(G.face);
                    render_present();
                }
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (e.button.button != SDL_BUTTON_LEFT) break;
            {
                int cx = e.button.x;
                int cy = e.button.y - render_menu_off();
                up_inside = (cx >= x0 && cx < x0 + 0x24 && cy >= 0x10 && cy < 0x28);
                done = 1;
            }
            break;
        case SDL_QUIT:
            g_quit = 1;
            done = 1;
            break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_CLOSE) { g_quit = 1; done = 1; }
            break;
        }
    }
    SDL_CaptureMouse(SDL_FALSE);

    if (up_inside && !g_quit) {
        G.face = 0;
        draw_face(0);
        gm_new_game();
    }
    return 1;
}

/* ---------------- Win95-style modal dialogs ---------------- */

#define CT_BTN    0
#define CT_EDIT   1
#define CT_STATIC 2
#define CT_ICON   3

typedef struct {
    int id, type, x, y, w, h, mark;
    const char *text;
} Ctl;

typedef int (*DlgHook)(int id, void *arg);   /* returns 1 to keep the dialog open */

static SDL_Surface *load_icon(void)
{
    static SDL_Surface *ic;
    if (!ic) {
        char path[600];
        snprintf(path, sizeof(path), "%s/icon_8.ico", render_asset_dir());
        ic = IMG_Load(path);
    }
    return ic;
}

static void set_win_icon(SDL_Window *w)
{
    SDL_Surface *ic = load_icon();
    if (!ic) return;
    SDL_SetWindowIcon(w, ic);
}

static void draw_button(SDL_Surface *s, const Ctl *c, int is_def)
{
    bevel_raise(s, c->x, c->y, c->w, c->h);
    if (is_def) {
        /* Win95 default button: extra black outline one pixel out */
        hline(s, c->x - 2, c->x + c->w + 1, c->y - 2, COL_TEXT_DK);
        hline(s, c->x - 2, c->x + c->w + 1, c->y + c->h + 1, COL_TEXT_DK);
        vline(s, c->x - 2, c->y - 2, c->y + c->h + 1, COL_TEXT_DK);
        vline(s, c->x + c->w + 1, c->y - 2, c->y + c->h + 1, COL_TEXT_DK);
        dotted_rect(s, c->x + 3, c->y + 3, c->w - 6, c->h - 6);
    }
    /* shrink font (like txt_surf_fit) when the label is wider than the button */
    TTF_Font *f = f_dlg;
    int tw = txt_w(c->text, f), th = txt_h(f), size = 12;
    while (size > 8 && tw > c->w - 6 && g_font_path[0]) {
        TTF_Font *nf = TTF_OpenFont(g_font_path, --size);
        if (!nf) break;
        if (f != f_dlg) TTF_CloseFont(f);
        f = nf;
        tw = txt_w(c->text, f);
        th = txt_h(f);
    }
    draw_text_mark(s, c->text, c->mark, c->x + (c->w - tw) / 2,
                   c->y + (c->h - th) / 2, col_text, f);
    if (f != f_dlg) TTF_CloseFont(f);
}

static void draw_edit(SDL_Surface *s, const Ctl *c, const char *text, int focused, int caret_on)
{
    bevel_sunken(s, c->x, c->y, c->w, c->h);
    int maxw = c->w - 8;
    int tw = txt_w(text, f_dlg);
    const char *draw = text;
    /* scroll to the tail when the text no longer fits */
    while (tw > maxw && *draw) { draw++; tw = txt_w(draw, f_dlg); }
    int th = txt_h(f_dlg);
    blit(s, txt_surf(draw, col_text, f_dlg), c->x + 4, c->y + (c->h - th) / 2);
    if (focused && caret_on)
        vline(s, c->x + 4 + tw + 1, c->y + 3, c->y + c->h - 4, COL_TEXT_DK);
}

static int run_dialog(const char *title, int dw, int dh, Ctl *ctls, int nc,
                      int def_id, int esc_id, char edits[][40], int n_edits,
                      int focus0, int max_len, int digits_only,
                      DlgHook hook, void *hook_arg)
{
    int px0, py0, pw, ph;
    SDL_GetWindowPosition(render_window(), &px0, &py0);
    SDL_GetWindowSize(render_window(), &pw, &ph);
    int x = px0 + (pw - dw) / 2;
    int y = py0 + (ph - dh) / 2;

    SDL_Window *w = SDL_CreateWindow(title, x, y, dw, dh, SDL_WINDOW_HIDDEN);
    if (!w) { SDL_Log("dialog: %s", SDL_GetError()); return esc_id; }
    set_win_icon(w);
    SDL_ShowWindow(w);
    Uint32 dlgid = SDL_GetWindowID(w);
    Uint32 mainid = SDL_GetWindowID(render_window());

    /* map CT_EDIT controls to edit buffer indices */
    int edit_of[16];
    int eidx = 0;
    for (int i = 0; i < nc; i++) {
        edit_of[i] = -1;
        if (ctls[i].type == CT_EDIT && eidx < n_edits) edit_of[i] = eidx++;
    }
    int focus = focus0;
    int ret = esc_id, done = 0;

    while (!done && !g_quit) {
        SDL_Surface *s = SDL_GetWindowSurface(w);
        fill_r(s, 0, 0, dw, dh, COL_FACE);
        int caret_on = ((SDL_GetTicks() / 500) & 1) == 0;
        for (int i = 0; i < nc; i++) {
            Ctl *c = &ctls[i];
            switch (c->type) {
            case CT_BTN:
                draw_button(s, c, c->id == def_id);
                break;
            case CT_EDIT:
                draw_edit(s, c, edit_of[i] >= 0 ? edits[edit_of[i]] : "",
                          focus == edit_of[i], caret_on);
                break;
            case CT_STATIC:
                if (c->w > 0)
                    blit(s, txt_surf_fit(c->text, col_text, c->w), c->x, c->y);
                else
                    blit(s, txt_surf(c->text, col_text, f_dlg), c->x, c->y);
                break;
            case CT_ICON: {
                SDL_Surface *ic = load_icon();
                if (ic) {
                    SDL_Rect r = {c->x, c->y, 0, 0};
                    SDL_BlitSurface(ic, NULL, s, &r);
                }
                break;
            }
            }
        }
        SDL_UpdateWindowSurface(w);

        SDL_Event e;
        if (!SDL_WaitEventTimeout(&e, 120)) continue;   /* caret blink refresh */

        if (e.type == SDL_WINDOWEVENT && e.window.windowID != dlgid &&
            e.window.windowID != mainid)
            continue;

        switch (e.type) {
        case SDL_QUIT:
            g_quit = 1;
            ret = -1;
            done = 1;
            break;
        case SDL_WINDOWEVENT:
            if (e.window.event != SDL_WINDOWEVENT_CLOSE) break;
            if (e.window.windowID == dlgid) {
                ret = esc_id;      /* X sends IDCANCEL to the dialog proc */
                done = 1;
            } else {
                g_quit = 1;
                ret = -1;
                done = 1;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (e.button.button != SDL_BUTTON_LEFT) break;
            for (int i = 0; i < nc; i++) {
                Ctl *c = &ctls[i];
                if (c->type == CT_BTN &&
                    e.button.x >= c->x && e.button.x < c->x + c->w &&
                    e.button.y >= c->y && e.button.y < c->y + c->h) {
                    if (hook && hook(c->id, hook_arg)) break;   /* stay open */
                    ret = c->id;
                    done = 1;
                    break;
                }
                if (c->type == CT_EDIT && edit_of[i] >= 0 &&
                    e.button.x >= c->x && e.button.x < c->x + c->w &&
                    e.button.y >= c->y && e.button.y < c->y + c->h) {
                    focus = edit_of[i];
                    break;
                }
            }
            break;
        case SDL_KEYDOWN:
            switch (e.key.keysym.sym) {
            case SDLK_ESCAPE:
                ret = esc_id;
                done = 1;
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                ret = def_id;
                done = 1;
                break;
            case SDLK_TAB:
                if (n_edits > 0)
                    focus = (focus < 0) ? 0 : (focus + 1) % n_edits;
                break;
            case SDLK_BACKSPACE:
                if (focus >= 0 && focus < n_edits) {
                    int l = (int)strlen(edits[focus]);
                    if (l > 0) edits[focus][l - 1] = 0;
                }
                break;
            }
            break;
        case SDL_TEXTINPUT:
            if (focus >= 0 && focus < n_edits) {
                char ch = e.text.text[0];
                if (!digits_only || (ch >= '0' && ch <= '9')) {
                    if ((int)strlen(edits[focus]) < max_len) {
                        int l = (int)strlen(edits[focus]);
                        edits[focus][l] = ch;
                        edits[focus][l + 1] = 0;
                    }
                }
            }
            break;
        }
    }
    SDL_DestroyWindow(w);
    return ret;
}

/* ---------------- command handlers ---------------- */

/* 1b49: Custom Field dialog, then always difficulty=3 + dirty + new game */
static void ui_cmd_custom(void)
{
    char edits[3][40];
    snprintf(edits[0], 40, "%d", G.pend_height);
    snprintf(edits[1], 40, "%d", G.pend_width);
    snprintf(edits[2], 40, "%d", G.pend_mines);

    Ctl ctls[] = {
        {1,   CT_BTN,    120, 32, 60, 26, -1, "OK"},
        {2,   CT_BTN,    120, 75, 60, 26, -1, "Cancel"},
        {141, CT_EDIT,    60, 32, 37, 19, -1, NULL},
        {142, CT_EDIT,    60, 57, 37, 19, -1, NULL},
        {143, CT_EDIT,    60, 81, 37, 19, -1, NULL},
        {0,   CT_STATIC,  15, 36,  0,  0,  0, "Height:"},
        {0,   CT_STATIC,  15, 60,  0,  0,  0, "Width:"},
        {0,   CT_STATIC,  15, 84,  0,  0,  0, "Mines:"},
    };

    int r = run_dialog("Custom Field", 195, 138, ctls, 8, 1, 2,
                       edits, 3, -1, 3, 1, NULL, NULL);

    if (r == 1) {   /* OK / Enter: 15a6 IDs 1/100 apply 3df6 clamps */
        int h = atoi(edits[0]);
        int w = atoi(edits[1]);
        int m = atoi(edits[2]);
        if (h < 9) h = 9;
        if (h > 24) h = 24;
        if (w < 9) w = 9;
        if (w > 30) w = 30;
        int mmax = (w - 1) * (h - 1);
        if (mmax > 999) mmax = 999;
        if (m < 10) m = 10;
        if (m > mmax) m = mmax;
        G.pend_height = h;
        G.pend_width = w;
        G.pend_mines = m;
    }
    /* 1b49 quirk: always applies, even on Cancel/X */
    G.difficulty = 3;
    G.dirty = 1;
    gm_new_game();
}

/* 1b81: name entry (dialog 600) */
void ui_ask_name(void)
{
    static const char *lines[3][3] = {
        {"You have the fastest time", "for beginner level.", "Please enter your name."},
        {"You have the fastest time", "for intermediate level.", "Please enter your name."},
        {"You have the fastest time", "for expert level.", "Please enter your name."},
    };
    int d = G.difficulty;
    if (d < 0 || d > 2) d = 2;

    char edits[1][40];
    snprintf(edits[0], 40, "%s", G.best_name[d]);

    Ctl ctls[] = {
        {1,   CT_BTN,    48, 115, 54, 26, -1, "OK"},
        {602, CT_EDIT,   18,  83, 117, 19, -1, NULL},
        {601, CT_STATIC,  6,   6, 138,  0, -1, lines[d][0]},
        {601, CT_STATIC,  6,  22, 138,  0, -1, lines[d][1]},
        {601, CT_STATIC,  6,  38, 138,  0, -1, lines[d][2]},
    };

    /* 181f: IDs 1/2/100/109 all save the name; X close sends 2 */
    run_dialog("Minesweeper", 150, 163, ctls, 5, 1, 2, edits, 1, 0, 32, 0, NULL, NULL);
    snprintf(G.best_name[d], sizeof(G.best_name[d]), "%.*s", 32, edits[0]);
    G.dirty = 1;
}

/* 1baa: best times (dialog 700) */
typedef struct { char times[3][32]; } BestDlg;

static int best_hook(int id, void *arg)
{
    BestDlg *b = (BestDlg *)arg;
    if (id == 707) {   /* Reset Scores */
        for (int i = 0; i < 3; i++) {
            G.best_time[i] = 999;
            snprintf(G.best_name[i], sizeof(G.best_name[i]), "Anonymous");
            snprintf(b->times[i], 32, "%d seconds", G.best_time[i]);
        }
        G.dirty = 1;
        return 1;   /* keep the dialog open, refreshed */
    }
    return 0;
}

void ui_best_times(void)
{
    BestDlg b;
    for (int i = 0; i < 3; i++)
        snprintf(b.times[i], 32, "%d seconds", G.best_time[i]);

    Ctl ctls[] = {
        {1,   CT_BTN,    172, 89, 45, 19, -1, "OK"},
        {707, CT_BTN,     37, 89, 75, 19,  0, "Reset Scores"},
        {708, CT_STATIC,  15, 24,  72,  0, -1, "Beginner:"},
        {709, CT_STATIC,  15, 41,  72,  0, -1, "Intermediate:"},
        {710, CT_STATIC,  15, 57,  72,  0, -1, "Expert:"},
        {701, CT_STATIC,  90, 24,  66,  0, -1, b.times[0]},
        {703, CT_STATIC,  90, 41,  66,  0, -1, b.times[1]},
        {705, CT_STATIC,  90, 57,  66,  0, -1, b.times[2]},
        {702, CT_STATIC, 172, 24,  90,  0, -1, G.best_name[0]},
        {704, CT_STATIC, 172, 41,  90,  0, -1, G.best_name[1]},
        {706, CT_STATIC, 172, 57,  90,  0, -1, G.best_name[2]},
    };
    run_dialog("Fastest Mine Sweepers", 255, 122, ctls, 11, 1, 1,
               NULL, 0, -1, 0, 0, best_hook, &b);
}

/* 3d1d: ShellAbout */
static void ui_about(void)
{
    Ctl ctls[] = {
        {8, CT_ICON,   20, 20, 32, 32, -1, NULL},
        {0, CT_STATIC, 70, 22, 180,  0, -1, "Minesweeper"},
        {0, CT_STATIC, 70, 46, 180,  0, -1, "by Robert Donner and Curt Johnson"},
        {1, CT_BTN,   100, 105, 60, 24, -1, "OK"},
    };
    run_dialog("About Minesweeper", 260, 145, ctls, 4, 1, 1, NULL, 0, -1, 0, 0, NULL, NULL);
}

/* 3d76: WinHelp equivalents */
void ui_help(int cmd)
{
    if (cmd == 593) { ui_about(); return; }
    static const char *cands[] = {"WINMINE.HLP", "winmine.hlp", "WINMINE.CHM", "NTHelp.chm"};
    for (unsigned i = 0; i < sizeof(cands) / sizeof(cands[0]); i++) {
        char path[600];
        snprintf(path, sizeof(path), "%s/%s", render_asset_dir(), cands[i]);
        if (access(path, R_OK) == 0) {
            char sys[700];
            snprintf(sys, sizeof(sys), "xdg-open '%s' >/dev/null 2>&1 &", path);
            if (system(sys) == -1) SDL_Log("help: spawn failed");
            return;
        }
    }
    /* also try the game directory itself (assets/ is a subdir of it) */
    for (unsigned i = 0; i < sizeof(cands) / sizeof(cands[0]); i++) {
        if (access(cands[i], R_OK) == 0) {
            char sys[700];
            snprintf(sys, sizeof(sys), "xdg-open '%s' >/dev/null 2>&1 &", cands[i]);
            if (system(sys) == -1) SDL_Log("help: spawn failed");
            return;
        }
    }
}

/* WM_COMMAND dispatch (wndproc switch) */
void ui_command(int id)
{
    switch (id) {
    case 510:                         /* New F2: no dirty, no 3ce5 */
        gm_new_game();
        break;
    case 512:                         /* Exit */
        g_quit = 1;
        break;
    case 521:
    case 522:
    case 523: {
        static const int presets[3][3] = { /* DAT_01005010 {mines,height,width} */
            {10, 9, 9}, {40, 16, 16}, {99, 16, 30}
        };
        G.difficulty = id - 521;
        G.pend_mines  = presets[G.difficulty][0];
        G.pend_height = presets[G.difficulty][1];
        G.pend_width  = presets[G.difficulty][2];
        gm_new_game();
        G.dirty = 1;
        ui_menu_refresh(G.menu_visible);
        break;
    }
    case 524:                         /* Custom... -> 1b49 */
        ui_cmd_custom();
        break;
    case 526:                         /* Sound */
        if (G.sound == 0)
            G.sound = sound_init() ? 3 : 2;   /* 38c2: 2 + success */
        else {
            sound_stop();                     /* 38d7 */
            G.sound = 0;
        }
        G.dirty = 1;
        ui_menu_refresh(G.menu_visible);
        break;
    case 527:                         /* Marks */
        G.marks ^= 1;
        G.dirty = 1;
        ui_menu_refresh(G.menu_visible);
        break;
    case 528:                         /* Best Times: no dirty, no 3ce5 */
        ui_best_times();
        break;
    case 529: {                       /* Color: 2607 + 2414 + 2af0 */
        G.color ^= 1;
        if (render_reload_assets() != 0) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Minesweeper Error",
                                     "Unable to load resources.", render_window());
            g_quit = 1;
            break;
        }
        G.dirty = 1;
        ui_menu_refresh(G.menu_visible);
        break;
    }
    case 590:
    case 591:
    case 592:
    case 593:
        ui_help(id);
        break;
    }
}
