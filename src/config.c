/* config.c — settings persistence.
 * Original: registry HKCU\Software\Microsoft\winmine (2bc2 load, 2dab save)
 * plus entpack.ini first-run defaults (3ab0). Ported to a plain key=value
 * file at ~/.config/winmine/winmine.cfg with the same keys and clamps. */
#include "winmine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static char cfg_path[512];

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static void build_path(void)
{
    const char *xdg = getenv("XDG_CONFIG_HOME");
    char base[400];
    if (xdg && *xdg)
        snprintf(base, sizeof(base), "%s", xdg);
    else
        snprintf(base, sizeof(base), "%s/.config", getenv("HOME") ? getenv("HOME") : ".");
    snprintf(cfg_path, sizeof(cfg_path), "%s/winmine/winmine.cfg", base);
}

/* 2b27-style clamp helpers */
static int clamp_height(int v) { return clampi(v, 9, 25); }   /* default 9  */
static int clamp_width(int v)  { return clampi(v, 9, 30); }   /* default 9  */
static int clamp_diff(int v)   { return clampi(v, 0, 3); }    /* default 0  */
static int clamp_mines(int v)  { return clampi(v, 10, 999); } /* default 10 */
static int clamp_pos(int v)    { return clampi(v, 0, 1024); } /* default 80 */
static int clamp_sound(int v)  { return clampi(v, 0, 3); }    /* default 0  */
static int clamp_mark(int v)   { return clampi(v, 0, 1); }    /* default 1  */
static int clamp_menu(int v)   { return clampi(v, 0, 2); }    /* default 0  */
static int clamp_time(int v)   { return clampi(v, 0, 999); }  /* default 999*/

static void parse_line(const char *line)
{
    const char *eq = strchr(line, '=');
    if (!eq) return;
    char key[32];
    size_t kn = (size_t)(eq - line);
    if (kn >= sizeof(key)) return;
    memcpy(key, line, kn);
    key[kn] = 0;
    const char *val = eq + 1;
    int iv = atoi(val);

    if (!strcmp(key, "Difficulty")) {
        G.difficulty = clamp_diff(iv);
    } else if (!strcmp(key, "Mines")) {
        G.pend_mines = clamp_mines(iv);
    } else if (!strcmp(key, "Height")) {
        /* 2bc2: DAT_01005338 = DAT_010056a8 = clamp(Height) */
        G.height = G.pend_height = clamp_height(iv);
    } else if (!strcmp(key, "Width")) {
        /* 2bc2: DAT_01005334 = DAT_010056ac = clamp(Width) */
        G.width = G.pend_width = clamp_width(iv);
    } else if (!strcmp(key, "Xpos")) {
        G.win_x = clamp_pos(iv);
    } else if (!strcmp(key, "Ypos")) {
        G.win_y = clamp_pos(iv);
    } else if (!strcmp(key, "Sound")) {
        G.sound = clamp_sound(iv);
    } else if (!strcmp(key, "Mark")) {
        G.marks = clamp_mark(iv);
    } else if (!strcmp(key, "Menu")) {
        G.menu_visible = clamp_menu(iv);
    } else if (!strcmp(key, "Tick")) {
        /* DAT_010056c0: loaded but unused by the original */
    } else if (!strcmp(key, "Color")) {
        G.color = iv ? 1 : 0;
    } else if (!strcmp(key, "Time1")) {
        G.best_time[0] = clamp_time(iv);
    } else if (!strcmp(key, "Name1")) {
        snprintf(G.best_name[0], sizeof(G.best_name[0]), "%.*s", 32, val);
    } else if (!strcmp(key, "Time2")) {
        G.best_time[1] = clamp_time(iv);
    } else if (!strcmp(key, "Name2")) {
        snprintf(G.best_name[1], sizeof(G.best_name[1]), "%.*s", 32, val);
    } else if (!strcmp(key, "Time3")) {
        G.best_time[2] = clamp_time(iv);
    } else if (!strcmp(key, "Name3")) {
        snprintf(G.best_name[2], sizeof(G.best_name[2]), "%.*s", 32, val);
    }
    /* "AlreadyPlayed" is read and ignored */
}

void config_load(void)
{
    build_path();
    FILE *f = fopen(cfg_path, "r");
    if (!f) return; /* 3ab0 defaults already in gm_init */
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = 0;
        parse_line(line);
    }
    fclose(f);
}

/* 2dab: saves exactly this key set (Menu and Tick are never saved) */
void config_save(void)
{
    build_path();
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", cfg_path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = 0;
        mkdir(dir, 0755);
    }
    FILE *f = fopen(cfg_path, "w");
    if (!f) return;
    fprintf(f, "Difficulty=%d\n", G.difficulty);
    fprintf(f, "Height=%d\n", G.pend_height);
    fprintf(f, "Width=%d\n", G.pend_width);
    fprintf(f, "Mines=%d\n", G.pend_mines);
    fprintf(f, "Mark=%d\n", G.marks);
    fprintf(f, "AlreadyPlayed=1\n");
    fprintf(f, "Color=%d\n", G.color);
    fprintf(f, "Sound=%d\n", G.sound);
    fprintf(f, "Xpos=%d\n", G.win_x);
    fprintf(f, "Ypos=%d\n", G.win_y);
    fprintf(f, "Time1=%d\n", G.best_time[0]);
    fprintf(f, "Name1=%s\n", G.best_name[0]);
    fprintf(f, "Time2=%d\n", G.best_time[1]);
    fprintf(f, "Name2=%s\n", G.best_name[1]);
    fprintf(f, "Time3=%d\n", G.best_time[2]);
    fprintf(f, "Name3=%s\n", G.best_name[2]);
    fclose(f);
}
