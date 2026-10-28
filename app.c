#include "app.h"
#include "plat.h"
#include "gen3.h"
#include "store.h"
#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define SCREEN_W 480
#define SCREEN_H 272
#define SLOT_SIZE 32
#define SLOT_PAD  2
#define GRID_COLS 5
#define GRID_ROWS 6
#define MAX_FOUND 64
#define MAX_DEPTH 6
#define MAX_VISIT 30000

/* Layout constants */
#define TOP_H     22
#define BOT_H     28
#define PANE_GAP  12
#define GRID_W    (GRID_COLS * (SLOT_SIZE + SLOT_PAD) - SLOT_PAD)
#define GRID_H    (GRID_ROWS * (SLOT_SIZE + SLOT_PAD) - SLOT_PAD)
#define LEFT_X    16
#define RIGHT_X   (SCREEN_W - 16 - GRID_W)
#define GRID_Y    (TOP_H + 18)

/* ------------------------------------------------------------------ state */
typedef struct {
    char    path[256];
    char    fname[64];
    G3Save  sv;
    Store   st;
    int     focus;          /* 0 = Leaf Green save, 1 = ROM storage */
    int     box[2], cur[2]; /* box index and slot index (0-29) */
    int     dirty[2];
    int     gain[2];
    int     backed_up[2];
    int     holding;        /* -1 = none, else source side (0/1) */
    int     hold_slot;      /* absolute slot index while holding */
    char    msg[64];
} App;

static const char *SIDE_NAME[2] = { "LeafGreen", "Storage" };

static void set_msg(App *a, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(a->msg, sizeof a->msg, fmt, ap);
    va_end(ap);
}

static uint8_t *side_mons(App *a, int s) {
    return s == 0 ? g3_save_mons(&a->sv) : a->st.mons;
}

static int box_count(const uint8_t *mons, int box) {
    int c = 0;
    for (int i = 0; i < G3_BOX_SLOTS; i++)
        if (g3_mon_occupied(mons + (box * G3_BOX_SLOTS + i) * G3_MON_SIZE)) c++;
    return c;
}

static int mon_national(const uint8_t *mon) {
    if (!g3_mon_occupied(mon)) return 0;
    return g3_species_national(g3_mon_species(mon));
}

/* ------------------------------------------------------------- drawing */
static void draw_slot(int x, int y, const uint8_t *mon, int selected, int holding_here) {
    uint32_t bg = g3_mon_occupied(mon) ? C_SLOT : C_SLOT_EMPTY;
    plat_rect(x, y, SLOT_SIZE, SLOT_SIZE, bg);

    int nat = mon_national(mon);
    if (nat > 0)
        plat_sprite(x, y, nat);

    if (selected) {
        plat_rect_border(x - 1, y - 1, SLOT_SIZE + 2, SLOT_SIZE + 2, C_SLOT_H, 2);
    }
    if (holding_here) {
        plat_rect_border(x - 1, y - 1, SLOT_SIZE + 2, SLOT_SIZE + 2, C_ACCENT, 2);
    }
}

static void draw_grid(App *a, int side, int ox, int oy) {
    const uint8_t *mons = side_mons(a, side);
    int box = a->box[side];
    int cur = a->cur[side];
    int is_focus = (a->focus == side);

    for (int row = 0; row < GRID_ROWS; row++) {
        for (int col = 0; col < GRID_COLS; col++) {
            int slot = row * GRID_COLS + col;
            int x = ox + col * (SLOT_SIZE + SLOT_PAD);
            int y = oy + row * (SLOT_SIZE + SLOT_PAD);
            const uint8_t *mon = mons + (box * G3_BOX_SLOTS + slot) * G3_MON_SIZE;
            int selected = is_focus && (cur == slot);
            int holding_here = (a->holding == side && a->hold_slot == box * G3_BOX_SLOTS + slot);
            draw_slot(x, y, mon, selected, holding_here);
        }
    }
}

static void draw_main(App *a) {
    plat_begin_frame();
    plat_clear(C_BG);

    /* Top bar */
    plat_rect(0, 0, SCREEN_W, TOP_H, C_PANEL);
    char title[80];
    snprintf(title, sizeof title, "PKStorage  %s  slot %c #%lu%s",
             a->fname,
             a->sv.slot_off ? 'B' : 'A',
             (unsigned long)a->sv.counter,
             (a->dirty[0] || a->dirty[1]) ? "  [UNSAVED]" : "");
    plat_text(8, 7, title, C_TITLE);

    /* Pane headers */
    char left_h[48], right_h[48], tmp[12];
    g3_box_name(&a->sv, a->box[0], tmp);
    snprintf(left_h, sizeof left_h, "SAVE %02d/14 %s %d/30",
             a->box[0] + 1, tmp, box_count(side_mons(a, 0), a->box[0]));
    snprintf(right_h, sizeof right_h, "ROM %02d/14 STORAGE %d/30",
             a->box[1] + 1, box_count(side_mons(a, 1), a->box[1]));

    plat_text(LEFT_X, TOP_H + 4, left_h, a->focus == 0 ? C_LEFT : C_TEXT_DIM);
    plat_text(RIGHT_X, TOP_H + 4, right_h, a->focus == 1 ? C_RIGHT : C_TEXT_DIM);

    /* Grids */
    draw_grid(a, 0, LEFT_X, GRID_Y);
    draw_grid(a, 1, RIGHT_X, GRID_Y);

    /* Holding indicator */
    if (a->holding >= 0) {
        const uint8_t *hm = side_mons(a, a->holding) + a->hold_slot * G3_MON_SIZE;
        int nat = mon_national(hm);
        char hold[48];
        if (nat > 0)
            snprintf(hold, sizeof hold, "Holding: %s  (Cross=place  Circle=cancel)",
                     g3_species_name(nat));
        else
            snprintf(hold, sizeof hold, "Holding empty?");
        plat_text(8, SCREEN_H - BOT_H - 12, hold, C_ACCENT);
    }

    /* Message */
    if (a->msg[0])
        plat_text(8, SCREEN_H - BOT_H + 2, a->msg, C_WARN);

    /* Bottom controls */
    plat_rect(0, SCREEN_H - 16, SCREEN_W, 16, C_PANEL);
    if (a->holding >= 0)
        plat_text(8, SCREEN_H - 12, "CROSS Place   CIRCLE Cancel   L/R Box", C_TEXT_DIM);
    else
        plat_text(8, SCREEN_H - 12, "X Menu  SQ Pick  TRI Save  L/R Box  </> Pane  START Quit", C_TEXT_DIM);

    plat_end_frame();
}

/* ------------------------------------------------------------- dialogs */
static int ask_yes_no(const char *l1, const char *l2) {
    for (;;) {
        plat_begin_frame();
        plat_clear(C_BG);
        plat_text(40, 80, l1, C_TITLE);
        plat_text(40, 110, l2, C_TEXT);
        plat_text(40, 150, "X = Yes     O = No", C_TEXT_DIM);
        plat_end_frame();
        Btn b = plat_wait_button();
        if (b == B_CROSS) return 1;
        if (b == B_CIRCLE || b == B_QUIT) return 0;
    }
}

enum { ACT_NONE = 0, ACT_MOVE, ACT_COPY, ACT_DROP };

static int action_menu(App *a) {
    const uint8_t *mon = side_mons(a, a->focus) +
                         (a->box[a->focus] * G3_BOX_SLOTS + a->cur[a->focus]) * G3_MON_SIZE;
    if (!g3_mon_occupied(mon)) {
        set_msg(a, "Empty slot.");
        return ACT_NONE;
    }
    int nat = mon_national(mon);
    const char *name = g3_species_name(nat);
    const char *items[4];
    if (a->focus == 0) {
        items[0] = "Withdraw  - move to Storage";
        items[1] = "Store     - copy to Storage";
    } else {
        items[0] = "Deposit   - move to LeafGreen";
        items[1] = "Copy      - copy to LeafGreen";
    }
    items[2] = "Drop      - release";
    items[3] = "Cancel";
    int sel = 0;
    for (;;) {
        plat_begin_frame();
        plat_clear(C_BG);
        char title[64];
        snprintf(title, sizeof title, "%s: %s", SIDE_NAME[a->focus], name);
        plat_text(40, 40, title, C_TITLE);
        for (int i = 0; i < 4; i++) {
            uint32_t col = (i == sel) ? C_SLOT_H : C_TEXT;
            plat_text(60, 80 + i * 22, items[i], col);
            if (i == sel) plat_text(40, 80 + i * 22, ">", C_ACCENT);
        }
        plat_text(40, 200, "X Select   O Cancel", C_TEXT_DIM);
        plat_end_frame();
        Btn b = plat_wait_button();
        if (b == B_UP)   sel = (sel + 3) % 4;
        if (b == B_DOWN) sel = (sel + 1) % 4;
        if (b == B_CIRCLE || b == B_QUIT) return ACT_NONE;
        if (b == B_CROSS) return sel == 3 ? ACT_NONE : sel + 1;
    }
}

/* ------------------------------------------------------------- actions */
static void transfer(App *a, int copy) {
    int s = a->focus, d = 1 - s;
    uint8_t *sm = side_mons(a, s), *dm = side_mons(a, d);
    int si = a->box[s] * G3_BOX_SLOTS + a->cur[s];
    if (!g3_mon_occupied(sm + si * G3_MON_SIZE)) {
        set_msg(a, "That slot is empty.");
        return;
    }
    int db, ds;
    if (g3_find_empty(dm, a->box[d], &db, &ds) != 0) {
        set_msg(a, "%s is full.", SIDE_NAME[d]);
        return;
    }
    int nat = mon_national(sm + si * G3_MON_SIZE);
    if (g3_move(sm, si, dm, db * G3_BOX_SLOTS + ds, copy) != 0) {
        set_msg(a, "Transfer failed.");
        return;
    }
    a->dirty[d] = a->gain[d] = 1;
    if (!copy) a->dirty[s] = 1;
    const char *verb = s == 0 ? (copy ? "Stored" : "Withdrew") : (copy ? "Copied" : "Deposited");
    set_msg(a, "%s %s -> box %d slot %d", verb, g3_species_name(nat), db + 1, ds + 1);
}

static void drop_mon(App *a) {
    int s = a->focus;
    uint8_t *sm = side_mons(a, s);
    int si = a->box[s] * G3_BOX_SLOTS + a->cur[s];
    if (!g3_mon_occupied(sm + si * G3_MON_SIZE)) {
        set_msg(a, "Empty slot.");
        return;
    }
    int nat = mon_national(sm + si * G3_MON_SIZE);
    char q[64];
    snprintf(q, sizeof q, "Release %s?", g3_species_name(nat));
    if (!ask_yes_no("DROP Pokemon", q)) {
        set_msg(a, "Cancelled.");
        return;
    }
    g3_clear(sm, si);
    a->dirty[s] = 1;
    set_msg(a, "Released %s", g3_species_name(nat));
}

/* Pick-up / put-down */
static void start_hold(App *a) {
    int s = a->focus;
    int si = a->box[s] * G3_BOX_SLOTS + a->cur[s];
    if (!g3_mon_occupied(side_mons(a, s) + si * G3_MON_SIZE)) {
        set_msg(a, "Empty slot.");
        return;
    }
    a->holding = s;
    a->hold_slot = si;
    set_msg(a, "Picked up. Move cursor and press X to place.");
}

static void place_hold(App *a) {
    if (a->holding < 0) return;
    int s = a->holding;
    int d = a->focus;
    uint8_t *sm = side_mons(a, s);
    uint8_t *dm = side_mons(a, d);
    int si = a->hold_slot;
    int di = a->box[d] * G3_BOX_SLOTS + a->cur[d];

    if (s == d && si == di) {
        a->holding = -1;
        set_msg(a, "Cancelled.");
        return;
    }
    if (g3_mon_occupied(dm + di * G3_MON_SIZE)) {
        set_msg(a, "Destination occupied.");
        return;
    }
    /* Move (not copy) */
    if (g3_move(sm, si, dm, di, 0) != 0) {
        set_msg(a, "Move failed.");
        return;
    }
    a->dirty[s] = a->dirty[d] = 1;
    if (s != d) a->gain[d] = 1;
    a->holding = -1;
    set_msg(a, "Moved.");
}

static void cancel_hold(App *a) {
    a->holding = -1;
    set_msg(a, "Cancelled.");
}

static int write_side(App *a, int s) {
    const char *path = s == 0 ? a->path : plat_storage_path();
    char bak[300];
    if (!a->backed_up[s]) {
        int rc = file_backup(path, bak, sizeof bak);
        if (rc < 0) {
            set_msg(a, "Backup failed - NOT saved.");
            return -1;
        }
        a->backed_up[s] = 1;
    }
    int rc;
    if (s == 0) {
        g3_commit(&a->sv);
        rc = file_write_verified(a->path, a->sv.data, a->sv.size);
    } else {
        rc = store_save(&a->st, path);
    }
    if (rc != 0) {
        set_msg(a, "WRITE FAILED! Restore from .bak");
        return -1;
    }
    a->dirty[s] = a->gain[s] = 0;
    return 0;
}

static int do_save(App *a) {
    if (!a->dirty[0] && !a->dirty[1]) {
        set_msg(a, "Nothing to save.");
        return 0;
    }
    int first = (a->gain[0] && !a->gain[1]) ? 0 : 1;
    for (int k = 0; k < 2; k++) {
        int s = k == 0 ? first : 1 - first;
        if (a->dirty[s] && write_side(a, s) != 0) return -1;
    }
    set_msg(a, "Saved.");
    return 0;
}

/* ------------------------------------------------------------- main loop */
static int main_screen(App *a) {
    a->holding = -1;
    for (;;) {
        draw_main(a);
        Btn b = plat_wait_button();
        a->msg[0] = 0;
        int s = a->focus;
        int row = a->cur[s] / GRID_COLS;
        int col = a->cur[s] % GRID_COLS;

        if (a->holding >= 0) {
            /* Holding mode */
            switch (b) {
            case B_UP:    if (row > 0) a->cur[s] -= GRID_COLS; break;
            case B_DOWN:  if (row < GRID_ROWS-1) a->cur[s] += GRID_COLS; break;
            case B_LEFT:
                if (col > 0) a->cur[s]--;
                else if (s == 1) { a->focus = 0; a->cur[0] = row * GRID_COLS + (GRID_COLS-1); }
                break;
            case B_RIGHT:
                if (col < GRID_COLS-1) a->cur[s]++;
                else if (s == 0) { a->focus = 1; a->cur[1] = row * GRID_COLS; }
                break;
            case B_L: a->box[s] = (a->box[s] + G3_NUM_BOXES - 1) % G3_NUM_BOXES; break;
            case B_R: a->box[s] = (a->box[s] + 1) % G3_NUM_BOXES; break;
            case B_CROSS: place_hold(a); break;
            case B_CIRCLE: case B_QUIT: cancel_hold(a); break;
            default: break;
            }
            continue;
        }

        switch (b) {
        case B_QUIT: return 1;
        case B_UP:    if (row > 0) a->cur[s] -= GRID_COLS; break;
        case B_DOWN:  if (row < GRID_ROWS-1) a->cur[s] += GRID_COLS; break;
        case B_LEFT:
            if (col > 0) a->cur[s]--;
            else a->focus = 0;
            break;
        case B_RIGHT:
            if (col < GRID_COLS-1) a->cur[s]++;
            else a->focus = 1;
            break;
        case B_L: a->box[s] = (a->box[s] + G3_NUM_BOXES - 1) % G3_NUM_BOXES; break;
        case B_R: a->box[s] = (a->box[s] + 1) % G3_NUM_BOXES; break;
        case B_SQUARE: start_hold(a); break;
        case B_TRIANGLE: do_save(a); break;
        case B_CROSS: {
            int act = action_menu(a);
            if (act == ACT_MOVE) transfer(a, 0);
            else if (act == ACT_COPY) transfer(a, 1);
            else if (act == ACT_DROP) drop_mon(a);
            break;
        }
        case B_START:
            if (a->dirty[0] || a->dirty[1]) {
                for (;;) {
                    plat_begin_frame();
                    plat_clear(C_BG);
                    plat_text(40, 70, "Unsaved changes", C_TITLE);
                    plat_text(40, 110, "X = Save and quit", C_TEXT);
                    plat_text(40, 130, "O = Discard and quit", C_TEXT);
                    plat_text(40, 150, "TRI = Keep editing", C_TEXT);
                    plat_end_frame();
                    Btn c = plat_wait_button();
                    if (c == B_CROSS) { if (do_save(a) == 0) return 1; break; }
                    if (c == B_CIRCLE || c == B_QUIT) return 1;
                    if (c == B_TRIANGLE) break;
                }
            } else return 1;
            break;
        default: break;
        }
    }
}

/* -------------------------------------------------------------------- scan */
typedef struct { char path[256]; int leaf; } Found;
typedef struct { const char *dir; int depth; } ScanCtx;
static Found g_found[MAX_FOUND];
static int g_nfound, g_visited;

static int has_sav_ext(const char *n) {
    size_t l = strlen(n);
    return l > 4 && n[l-4] == '.' && (n[l-3] | 32) == 's' &&
           (n[l-2] | 32) == 'a' && (n[l-1] | 32) == 'v';
}

static int contains_leaf(const char *s) {
    for (; *s; s++)
        if ((s[0] | 32) == 'l' && (s[1] | 32) == 'e' &&
            (s[2] | 32) == 'a' && (s[3] | 32) == 'f') return 1;
    return 0;
}

static void join_path(char *out, size_t n, const char *dir, const char *name) {
    size_t l = strlen(dir);
    snprintf(out, n, "%s%s%s", dir, (l > 0 && dir[l-1] == '/') ? "" : "/", name);
}

static void scan_dir(const char *dir, int depth);

static int scan_cb(const char *name, int is_dir, long size, void *ud) {
    ScanCtx *c = (ScanCtx *)ud;
    if (++g_visited > MAX_VISIT) return 1;
    char full[256];
    join_path(full, sizeof full, c->dir, name);
    if (is_dir) {
        if (c->depth < MAX_DEPTH) scan_dir(full, c->depth + 1);
        return g_visited > MAX_VISIT;
    }
    if (has_sav_ext(name) && size >= (long)G3_SAVE_SIZE_MIN && g_nfound < MAX_FOUND) {
        snprintf(g_found[g_nfound].path, sizeof g_found[g_nfound].path, "%s", full);
        g_found[g_nfound].leaf = contains_leaf(name);
        g_nfound++;
    }
    return 0;
}

static void scan_dir(const char *dir, int depth) {
    ScanCtx c = { dir, depth };
    plat_walk_dir(dir, scan_cb, &c);
}

static void scan_all(void) {
    plat_begin_frame();
    plat_clear(C_BG);
    plat_text(40, 120, "Scanning for Pokemon saves...", C_TITLE);
    plat_end_frame();
    g_nfound = 0; g_visited = 0;
    scan_dir(plat_scan_root(), 0);
}

static const char *base_name(const char *p) {
    const char *b = p;
    for (; *p; p++) if (*p == '/' || *p == ':') b = p + 1;
    return b;
}

static int select_file(void) {
    int show_all = 0, sel = 0, top = 0;
    for (;;) {
        int idx[MAX_FOUND], n = 0;
        for (int i = 0; i < g_nfound; i++)
            if (show_all || g_found[i].leaf) idx[n++] = i;
        if (sel >= n) sel = n ? n - 1 : 0;

        plat_begin_frame();
        plat_clear(C_BG);
        plat_text(16, 10, show_all ? "All .sav files" : "LeafGreen saves", C_TITLE);

        if (n == 0) {
            plat_text(16, 50, "No matching .sav found.", C_WARN);
            plat_text(16, 70, "Put a 128KB LeafGreen save on the stick", C_TEXT_DIM);
            plat_text(16, 86, "or press SQ to show every .sav", C_TEXT_DIM);
        }

        int rows = 18;
        if (sel < top) top = sel;
        if (sel >= top + rows) top = sel - rows + 1;
        for (int i = top; i < n && i < top + rows; i++) {
            const char *p = g_found[idx[i]].path;
            size_t l = strlen(p);
            char line[60];
            if (l > 55) snprintf(line, sizeof line, "...%s", p + l - 52);
            else snprintf(line, sizeof line, "%s", p);
            uint32_t col = (i == sel) ? C_SLOT_H : C_TEXT;
            if (i == sel) plat_text(8, 32 + (i - top) * 12, ">", C_ACCENT);
            plat_text(20, 32 + (i - top) * 12, line, col);
        }
        plat_text(16, SCREEN_H - 20, "X Open  SQ Toggle  TRI Rescan  START Quit", C_TEXT_DIM);
        plat_end_frame();

        Btn b = plat_wait_button();
        if (b == B_QUIT || b == B_START) return -1;
        if (b == B_UP && n)   sel = (sel + n - 1) % n;
        if (b == B_DOWN && n) sel = (sel + 1) % n;
        if (b == B_SQUARE)    { show_all = !show_all; sel = top = 0; }
        if (b == B_TRIANGLE)  return -2;
        if (b == B_CROSS && n) return idx[sel];
    }
}

static void show_error(const char *l1, const char *l2) {
    for (;;) {
        plat_begin_frame();
        plat_clear(C_BG);
        plat_text(40, 90, l1, C_WARN);
        plat_text(40, 120, l2, C_TEXT);
        plat_text(40, 160, "Press any button.", C_TEXT_DIM);
        plat_end_frame();
        plat_wait_button();
        return;
    }
}

int app_run(void) {
    App *a = (App *)calloc(1, sizeof *a);
    if (!a) return 1;
    a->holding = -1;

    for (;;) {
        scan_all();
        int pick;
        for (;;) {
            pick = select_file();
            if (pick == -1) { free(a); return 0; }
            if (pick == -2) break; /* rescan */
            if (pick >= 0) break;
        }
        if (pick < 0) continue;

        size_t sz = 0;
        uint8_t *data = file_read_all(g_found[pick].path, &sz);
        if (!data) {
            show_error("Cannot read file", g_found[pick].path);
            continue;
        }
        char err[128];
        if (g3_load(&a->sv, data, sz, err, sizeof err) != 0) {
            free(data);
            show_error("Not a valid Gen 3 save", err);
            continue;
        }
        snprintf(a->path, sizeof a->path, "%s", g_found[pick].path);
        snprintf(a->fname, sizeof a->fname, "%s", base_name(g_found[pick].path));
        a->box[0] = a->box[1] = a->cur[0] = a->cur[1] = 0;
        a->focus = 0;
        a->dirty[0] = a->dirty[1] = a->gain[0] = a->gain[1] = 0;
        a->backed_up[0] = a->backed_up[1] = 0;
        a->msg[0] = 0;

        if (store_load(&a->st, plat_storage_path(), err, sizeof err) < 0) {
            show_error("Storage file problem", err);
            g3_free(&a->sv);
            continue;
        }

        int quit = main_screen(a);
        g3_free(&a->sv);
        if (quit) break;
    }
    free(a);
    return 0;
}
