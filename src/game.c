/* game.c - core Minesweeper logic, ported from WINMINE.EXE
 * Function mapping (original address -> port): *   2ed5 ClearGrid      -> gm_clear_grid
 *   367a NewGame        -> gm_new_game
 *   2f3b AdjMines       -> gm_neighbors(x,y,0x80,0x80)
 *   3119 AdjFlags       -> gm_neighbors(x,y,0x1f,0xe)
 *   3008 OpenCell       -> gm_open_cell
 *   3084 FloodFill      -> gm_flood
 *   316b PressCell      -> gm_press
 *   31a0 UnpressCell    -> gm_unpress
 *   31d4 MoveCursor     -> gm_move_cursor
 *   2eab SetCell        -> gm_set_cell
 *   2f80 RevealAll      -> gm_reveal_all
 *   347c EndGame        -> gm_endgame
 *   3512 ClickCell      -> gm_click
 *   35b7 ChordCell      -> gm_chord
 *   374f FlagCell       -> gm_flag
 *   37e1 CommitMove     -> gm_commit
 *   346a CounterAdd     -> gm_counter_add
 *   2fe0 TimerTick      -> gm_timer_tick
 *   341c Pause          -> gm_pause
 *   344c Resume         -> gm_resume
 */
#include "winmine.h"
#include <stdlib.h>

Game G;
int g_debug = 0;

/* 2ed5 ClearGrid: whole grid covered, border ring = 0x10 */
void gm_clear_grid(void)
{
    int x, y;
    for (int i = 0; i < GRID_COLS * GRID_ROWS; i++)
        G.grid[i] = ST_COVERED;
    for (x = 0; x <= G.width + 1; x++) {
        G.grid[x] = ST_BORDER;
        G.grid[x + (G.height + 1) * GRID_COLS] = ST_BORDER;
    }
    for (y = 0; y <= G.height + 1; y++) {
        G.grid[y * GRID_COLS] = ST_BORDER;
        G.grid[y * GRID_COLS + G.width + 1] = ST_BORDER;
    }
}

/* 367a NewGame */
void gm_new_game(void)
{
    DBG("NEW GAME: pend=(%d,%d,%d) active=(%d,%d)\n",
        G.pend_width, G.pend_height, G.pend_mines, G.width, G.height);
    int mode, left, x, y;

    G.timer_running = 0;
    mode = (G.pend_width == G.width && G.pend_height == G.height) ? 4 : 6;
    G.width = G.pend_width;
    G.height = G.pend_height;
    gm_clear_grid();
    G.face = 0;

    /* place mines at random playfield cells (1-based), no duplicates */
    left = G.pend_mines;
    do {
        do {
            x = rand() % G.width;      /* 3940: rand() % n */
            y = rand() % G.height;
        } while (G.grid[(x + 1) + (y + 1) * GRID_COLS] & F_MINE);
        G.grid[(x + 1) + (y + 1) * GRID_COLS] |= F_MINE;
        left--;
    } while (left != 0);

    G.safe_count = G.height * G.width - G.pend_mines;
    G.timer = 0;
    G.mine_counter = G.pend_mines;
    G.revealed = 0;
    G.flags = FL_STARTED;
    G.probe_active = 0;                /* XYZZY dot resets with each game */
    if (g_debug) {
        fprintf(stderr, "NEWGAME mode=%d w=%d h=%d pend=%d safe=%d\n",
                mode, G.width, G.height, G.pend_mines, G.safe_count);
        fprintf(stderr, "MINES:");
        for (int my = 1; my <= G.height; my++)
            for (int mx = 1; mx <= G.width; mx++)
                if (G.grid[mx + my * GRID_COLS] & F_MINE)
                    fprintf(stderr, " (%d,%d)", mx, my);
        fprintf(stderr, "\n");
    }
    gm_counter_add(0);                 /* 346a(0): refresh counter display */
    render_apply_window_size(mode);    /* 1950 */
}

/* 2f3b (mask 0x80,val 0x80) / 3119 (mask 0x1f,val 0xe): 3x3 neighborhood count */
int gm_neighbors(int x, int y, int mask, int val)
{
    int n = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if ((G.grid[(x + dx) + (y + dy) * GRID_COLS] & mask) == val)
                n++;
    return n;
}

/* 3008 OpenCell */
void gm_open_cell(int x, int y)
{
    int idx = x + y * GRID_COLS;
    int st;

    if (!(G.grid[idx] & F_REVEALED)) {
        st = G.grid[idx] & 0x1f;
        if (st != ST_BORDER && st != ST_FLAG) {
            G.revealed++;
            st = gm_neighbors(x, y, 0x80, 0x80);
            G.grid[idx] = st | F_REVEALED;
            draw_cell(x, y);
            if (st == 0) {
                /* push onto 100-slot ring queue (write index wraps at 100) */
                G.queue_x[G.queue_idx] = x;
                G.queue_y[G.queue_idx] = y;
                G.queue_idx++;
                if (G.queue_idx == 100)
                    G.queue_idx = 0;
            }
        }
    }
}

/* 3084 FloodFill: ring queue, read index starts at 1 */
void gm_flood(int x, int y)
{
    int r = 1, cx, cy;

    G.queue_idx = 1;
    gm_open_cell(x, y);
    if (G.queue_idx != 1) {
        do {
            cy = G.queue_y[r];
            cx = G.queue_x[r];
            gm_open_cell(cx - 1, cy - 1);
            gm_open_cell(cx,     cy - 1);
            gm_open_cell(cx + 1, cy - 1);
            gm_open_cell(cx - 1, cy);
            gm_open_cell(cx + 1, cy);
            gm_open_cell(cx - 1, cy + 1);
            gm_open_cell(cx,     cy + 1);
            gm_open_cell(cx + 1, cy + 1);
            r++;
            if (r == 100)
                r = 0;
        } while (r != G.queue_idx);
    }
}

/* 316b PressCell: covered->tile 0, question->tile 9 (pressed look) */
void gm_press(int x, int y)
{
    int idx = x + y * GRID_COLS;
    int st = G.grid[idx] & 0x1f;

    if (st == ST_QUESTION)
        st = 9;
    else if (st == ST_COVERED)
        st = 0;
    G.grid[idx] = (G.grid[idx] & 0xe0) | st;
}

/* 31a0 UnpressCell: inverse of gm_press */
void gm_unpress(int x, int y)
{
    int idx = x + y * GRID_COLS;
    int st = G.grid[idx] & 0x1f;

    if (st == 9)
        st = ST_QUESTION;
    else if (st == 0)
        st = ST_COVERED;
    G.grid[idx] = (G.grid[idx] & 0xe0) | st;
}

/* 31d4 MoveCursor: drag tracking with press/unpress of old/new cells.
 * In chord mode the whole 3x3 neighborhoods around old and new cursor are
 * pressed/unpressed (clamped to the playfield). */
void gm_move_cursor(int nx, int ny)
{
    int old_x = G.cursor_x, old_y = G.cursor_y;
    int old_ok, new_ok;
    int oy0, oy1, ox0, ox1, ny0, ny1, nx0, nx1;

    if (nx == G.cursor_x && ny == G.cursor_y)
        return;
    G.cursor_x = nx;
    G.cursor_y = ny;

    if (G.chord == 0) {
        /* single-cell press/unpress */
        if (old_x > 0 && old_y > 0 && old_x <= G.width && old_y <= G.height &&
            !(G.grid[old_x + old_y * GRID_COLS] & F_REVEALED)) {
            gm_unpress(old_x, old_y);
            draw_cell(old_x, old_y);
        }
        if (nx > 0 && ny > 0 && nx <= G.width && ny <= G.height &&
            !(G.grid[nx + ny * GRID_COLS] & F_REVEALED) &&
            (G.grid[nx + ny * GRID_COLS] & 0x1f) != ST_FLAG) {
            gm_press(nx, ny);
            draw_cell(nx, ny);
        }
        return;
    }

    /* chord drag: 3x3 blocks around old and new cursor */
    new_ok = !(nx < 1 || ny < 1 || G.width < nx || G.height < ny);
    old_ok = !(old_x < 1 || old_y < 1 || G.width < old_x || G.height < old_y);

    /* clamp helper mirrors the original's "< 2 -> 1" and "<= max -> max" */
    #define LO(v) ((v) < 2 ? 1 : (v))
    #define HI(v, m) ((m) <= (v) ? (m) : (v))
    oy0 = LO(old_y - 1); oy1 = HI(old_y + 1, G.height);
    ny0 = LO(ny - 1);    ny1 = HI(ny + 1, G.height);
    ox0 = LO(old_x - 1); ox1 = HI(old_x + 1, G.width);
    nx0 = LO(nx - 1);    nx1 = HI(nx + 1, G.width);

    if (old_ok && oy0 <= oy1) {
        for (int yy = oy0; yy <= oy1; yy++)
            for (int xx = ox0; xx <= ox1; xx++)
                if (!(G.grid[xx + yy * GRID_COLS] & F_REVEALED))
                    gm_unpress(xx, yy);
    }
    if (new_ok && ny0 <= ny1) {
        for (int yy = ny0; yy <= ny1; yy++)
            for (int xx = nx0; xx <= nx1; xx++)
                if (!(G.grid[xx + yy * GRID_COLS] & F_REVEALED))
                    gm_press(xx, yy);
    }
    if (old_ok)
        for (int yy = oy0; yy <= oy1; yy++)
            for (int xx = ox0; xx <= ox1; xx++)
                draw_cell(xx, yy);
    if (new_ok)
        for (int yy = ny0; yy <= ny1; yy++)
            for (int xx = nx0; xx <= nx1; xx++)
                draw_cell(xx, yy);
    #undef LO
    #undef HI
}

/* 2eab SetCell: replace low 5 bits (param may carry 0x40 too, e.g. 0x4c) */
void gm_set_cell(int x, int y, int tile)
{
    int idx = x + y * GRID_COLS;
    G.grid[idx] = (G.grid[idx] & 0xe0) | (tile & 0xff);
    draw_cell(x, y);
}

/* 2f80 RevealAll: tile = 10 on loss (mines shown), 14 on win (mines flagged).
 * Flags on safe cells become wrong-flag (0xb). */
void gm_reveal_all(int tile)
{
    for (int y = 1; y <= G.height; y++) {
        for (int x = 1; x <= G.width; x++) {
            int idx = x + y * GRID_COLS;
            unsigned char b = G.grid[idx];
            if (!(b & F_REVEALED)) {
                if ((signed char)b < 0) {          /* has mine */
                    if ((b & 0x1f) != ST_FLAG)
                        G.grid[idx] = (b & 0xe0) | (tile & 0xff);
                } else if ((b & 0x1f) == ST_FLAG) {
                    G.grid[idx] = (b & 0xeb) | 0xb;
                }
            }
        }
    }
    draw_field();                                  /* 272e */
}

/* 347c EndGame */
void gm_endgame(int win)
{
    {
        int cov = 0;
        for (int yy = 1; yy <= G.height; yy++)
            for (int xx = 1; xx <= G.width; xx++) {
                int b = G.grid[xx + yy * GRID_COLS];
                if (!(b & F_REVEALED) && !(b & F_MINE)) cov++;
            }
        DBG("ENDGAME win=%d timer=%d counter=%d revealed=%d safe=%d cov_safe=%d\n",
            win, G.timer, G.mine_counter, G.revealed, G.safe_count, cov);
    }
    G.timer_running = 0;
    G.face = (win != 0) + 2;                       /* 2 dead, 3 cool */
    draw_face(G.face);
    gm_reveal_all((win != 0) * 4 + 10);            /* 10 loss / 14 win */
    if (win && G.mine_counter != 0)
        gm_counter_add(-G.mine_counter);           /* counter to 0 on win */
    sound_play(3 - (win != 0));                    /* 2 win / 3 boom */
    G.flags = FL_ENDED;
    if (win && G.difficulty != 3) {                /* custom records nothing */
        if (G.timer < G.best_time[G.difficulty]) {
            G.best_time[G.difficulty] = G.timer;
            DBG("ASK_NAME diff=%d timer=%d best=%d\n", G.difficulty, G.timer, G.best_time[G.difficulty]);
            ui_ask_name();                         /* 1b81 */
            ui_best_times();                       /* 1baa */
        }
    }
}

/* 3512 ClickCell: open, first-click mine relocation, or explode */
void gm_click(int x, int y)
{
    int idx = x + y * GRID_COLS;

    if (!(G.grid[idx] & F_MINE)) {
        gm_flood(x, y);
        if (G.revealed != G.safe_count)
            return;
        gm_endgame(1);
        return;
    }
    if (G.revealed == 0) {
        /* first click on a mine: move it to the first mine-free cell found
         * scanning rows 1..h-1, cols 1..w-1 (original off-by-one quirk) */
        if (G.height < 2)
            return;
        for (int r = 1; r < G.height; r++) {
            for (int c = 1; c < G.width; c++) {
                if (!(G.grid[c + r * GRID_COLS] & F_MINE)) {
                    G.grid[idx] = ST_COVERED;
                    G.grid[c + r * GRID_COLS] |= F_MINE;
                    gm_flood(x, y);
                    return;
                }
            }
        }
        return;
    }
    gm_set_cell(x, y, 0x4c);                       /* exploded */
    gm_endgame(0);
}

/* 35b7 ChordCell: only fires when revealed number == adjacent flag count;
 * covered mines in the neighborhood explode (loss). */
void gm_chord(int x, int y)
{
    int idx = x + y * GRID_COLS;
    int b = G.grid[idx];
    int boom = 0;

    if (!(b & F_REVEALED) || (b & 0x1f) != gm_neighbors(x, y, 0x1f, 0xe)) {
        gm_move_cursor(-2, -2);
        return;
    }
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int nb = G.grid[(x + dx) + (y + dy) * GRID_COLS];
            if ((nb & 0x1f) == ST_FLAG || (signed char)nb >= 0)
                gm_flood(x + dx, y + dy);
            else {
                boom = 1;
                gm_set_cell(x + dx, y + dy, 0x4c);
            }
        }
    }
    if (boom) {
        gm_endgame(0);
        return;
    }
    if (G.revealed != G.safe_count)
        return;
    gm_endgame(1);
}

/* 374f FlagCell: covered->flag(-1), flag->question(if marks)/covered(+1),
 * question->covered (no counter change). */
void gm_flag(int x, int y)
{
    int idx, st, delta = 0;

    DBG("flag: (%d,%d) marks=%d menu_loop=%d\n", x, y, G.marks, G.menu_loop);
    if (x < 1 || y < 1 || G.width < x || G.height < y)
        return;
    idx = x + y * GRID_COLS;
    if (G.grid[idx] & F_REVEALED)
        return;
    st = G.grid[idx] & 0x1f;
    if (st == ST_FLAG) {
        delta = 1;
        st = (G.marks == 0) ? ST_COVERED : ST_QUESTION;
        gm_counter_add(delta);
    } else if (st == ST_QUESTION) {
        st = ST_COVERED;
    } else {
        st = ST_FLAG;
        delta = -1;
        gm_counter_add(delta);
    }
    gm_set_cell(x, y, st);
    if ((G.grid[idx] & 0x1f) == ST_FLAG && G.revealed == G.safe_count)
        gm_endgame(1);                             /* flagged last safe cell */
}

/* 37e1 CommitMove: called on button release while dragging */
void gm_commit(void)
{
    DBG("commit: cursor=(%d,%d) chord=%d flags=%x revealed=%d\n",
        G.cursor_x, G.cursor_y, G.chord, G.flags, G.revealed);
    if (G.cursor_x > 0 && G.cursor_y > 0 &&
        G.cursor_x <= G.width && G.cursor_y <= G.height) {
        if (G.revealed == 0 && G.timer == 0) {
            /* first move: start timer at 1 */
            sound_play(1);
            G.timer++;
            draw_timer();                          /* 28b5 */
            G.timer_running = 1;
        }
        if (!(G.flags & FL_STARTED)) {
            G.cursor_x = -2;
            G.cursor_y = -2;
        }
        if (G.chord == 0) {
            int idx = G.cursor_x + G.cursor_y * GRID_COLS;
            if (!(G.grid[idx] & F_REVEALED) && (G.grid[idx] & 0x1f) != ST_FLAG)
                gm_click(G.cursor_x, G.cursor_y);
        } else {
            gm_chord(G.cursor_x, G.cursor_y);
        }
    }
    draw_face(G.face);                             /* 2913 */
}

/* 346a CounterAdd */
void gm_counter_add(int d)
{
    G.mine_counter += d;
    DBG("counter_add(%d) -> %d\n", d, G.mine_counter);
    draw_counter();                                /* 2801 */
}

/* 2fe0 TimerTick (WM_TIMER) */
void gm_timer_tick(void)
{
    if (G.timer_running && G.timer < 999) {
        G.timer++;
        draw_timer();                              /* 28b5 */
        sound_play(1);                             /* tick sound each second */
    }
}

/* 341c Pause */
void gm_pause(void)
{
    sound_stop();                                  /* 38d7 */
    if (!(G.flags & FL_PAUSED))
        G.timer_saved = G.timer_running;
    if (G.flags & FL_STARTED)
        G.timer_running = 0;
    G.flags |= FL_PAUSED;
}

/* 344c Resume */
void gm_resume(void)
{
    if (G.flags & FL_STARTED)
        G.timer_running = G.timer_saved;
    G.flags &= ~FL_PAUSED;
}
