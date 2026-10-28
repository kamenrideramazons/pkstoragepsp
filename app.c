#include "app.h"
#include "plat.h"
#include "gen3.h"
#include "store.h"
#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define COLS      65      /* text columns used (PSP debug screen is ~68 wide) */
#define PANE_W    32
#define MAX_FOUND 64
#define MAX_DEPTH 6
#define MAX_VISIT 30000

/* ------------------------------------------------------------------ output */
static void put_line_ex(const char *s, int color, int newline) {
    char buf[COLS + 2];
    size_t n = strlen(s);
    if (n > COLS) n = COLS;
    memcpy(buf, s, n);
    memset(buf + n, ' ', COLS - n);
    buf[COLS] = newline ? '\n' : 0; buf[COLS + 1] = 0;
    plat_color(color);
    plat_puts(buf);
    plat_color(C_NORMAL);
}

static void put_line(const char *s, int color) { put_line_ex(s, color, 1); }

static void linef(int color, const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    put_line(buf, color);
}

static void pad_to(char *dst, const char *s, int w) {
    int n = (int)strlen(s);
    if (n > w) n = w;
    memcpy(dst, s, n);
    memset(dst + n, ' ', w - n);
    dst[w] = 0;
}

static void put_row2(const char *l, int lc, const char *r, int rc) {
    char a[PANE_W + 1], b[PANE_W + 1];
    pad_to(a, l, PANE_W);
    pad_to(b, r, PANE_W);
    plat_color(lc); plat_puts(a);
    plat_color(C_NORMAL); plat_puts(" ");
    plat_color(rc); plat_puts(b);
    plat_color(C_NORMAL); plat_puts("\n");
}

/* ------------------------------------------------------------------- state */
typedef struct {
    char    path[256];
    char    fname[64];
    G3Save  sv;
    Store   st;
    int     focus;          /* 0 = Leaf Green save, 1 = ROM storage */
    int     box[2], cur[2];
    int     dirty[2];       /* unsaved changes per side */
    int     gain[2];        /* side received a Pokemon since last save */
    int     backed_up[2];
    char    msg[COLS + 1];
} App;

static const char *SIDE_NAME[2] = { "Leaf Green save", "ROM storage" };

static void set_msg(App *a, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(a->msg, sizeof a->msg, fmt, ap);
    va_end(ap);
}

static uint8_t *side_mons(App *a, int s) { return s == 0 ? g3_save_mons(&a->sv) : a->st.mons; }

static int ieq(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'a' && x <= 'z') x -= 32;
        if (y >= 'a' && y <= 'z') y -= 32;
        if (x != y) return 0;
    }
    return *a == *b;
}

static void mon_label(const uint8_t *mon, char *out, size_t n) {
    if (!g3_mon_occupied(mon)) { snprintf(out, n, "-"); return; }
    const char *sn = g3_species_name(g3_species_national(g3_mon_species(mon)));
    char nick[11];
    g3_mon_nickname(mon, nick);
    const char *bad = g3_mon_checksum_ok(mon) ? "" : " !";
    if (g3_mon_is_egg(mon))        snprintf(out, n, "EGG (%s)%s", sn, bad);
    else if (ieq(nick, sn))        snprintf(out, n, "%s%s", sn, bad);
    else                           snprintf(out, n, "%s (%s)%s", nick, sn, bad);
}

static int box_count(const uint8_t *mons, int box) {
    int c = 0;
    for (int i = 0; i < G3_BOX_SLOTS; i++)
        if (g3_mon_occupied(mons + (box * G3_BOX_SLOTS + i) * G3_MON_SIZE)) c++;
    return c;
}

/* ------------------------------------------------------------- main screen */
static void draw_main(App *a) {
    char l[96], r[96], tmp[64];
    plat_clear();
    linef(C_TITLE, "PKStorage  %s  slot %c #%lu%s", a->fname, a->sv.slot_off ? 'B' : 'A',
          (unsigned long)a->sv.counter, (a->dirty[0] || a->dirty[1]) ? "  [UNSAVED]" : "");

    g3_box_name(&a->sv, a->box[0], tmp);
    snprintf(l, sizeof l, "SAVE %02d/14 %-8s %2d/30", a->box[0] + 1, tmp, box_count(side_mons(a, 0), a->box[0]));
    snprintf(r, sizeof r, "ROM  %02d/14 STORAGE %2d/30", a->box[1] + 1, box_count(side_mons(a, 1), a->box[1]));
    put_row2(l, a->focus == 0 ? C_HILITE : C_NORMAL, r, a->focus == 1 ? C_HILITE : C_NORMAL);

    for (int i = 0; i < G3_BOX_SLOTS; i++) {
        char *txt[2] = { l, r };
        int col[2];
        for (int s = 0; s < 2; s++) {
            const uint8_t *mon = side_mons(a, s) + (a->box[s] * G3_BOX_SLOTS + i) * G3_MON_SIZE;
            char lab[48];
            mon_label(mon, lab, sizeof lab);
            int cur = (a->cur[s] == i);
            snprintf(txt[s], 96, "%c%02d %s", cur ? (a->focus == s ? '>' : ':') : ' ', i + 1, lab);
            col[s] = cur ? (a->focus == s ? C_HILITE : C_DIM) : C_NORMAL;
        }
        put_row2(l, col[0], r, col[1]);
    }
    put_line(a->msg, C_WARN);
    /* last of the 34 rows: no newline, so the PSP debug screen never scrolls/wraps */
    put_line_ex("X Menu  SQ Quick-move  TRI Save  L/R Box  </>: Pane  START Quit", C_DIM, 0);
}

static int ask_yes_no(const char *l1, const char *l2) {
    plat_clear();
    linef(C_TITLE, "%s", l1);
    put_line("", C_NORMAL);
    put_line(l2, C_NORMAL);
    put_line("", C_NORMAL);
    put_line("X = Yes     O = No", C_DIM);
    for (;;) {
        Btn b = plat_wait_button();
        if (b == B_CROSS) return 1;
        if (b == B_CIRCLE || b == B_QUIT) return 0;
    }
}

enum { ACT_NONE = 0, ACT_MOVE, ACT_COPY, ACT_DROP };

static int action_menu(App *a) {
    const uint8_t *mon = side_mons(a, a->focus) + (a->box[a->focus] * G3_BOX_SLOTS + a->cur[a->focus]) * G3_MON_SIZE;
    char lab[48];
    mon_label(mon, lab, sizeof lab);
    const char *items[4];
    if (a->focus == 0) {
        items[0] = "Withdraw  - move to ROM storage";
        items[1] = "Store     - copy to ROM storage";
    } else {
        items[0] = "Deposit   - move to Leaf Green save";
        items[1] = "Copy      - copy to Leaf Green save";
    }
    items[2] = "Drop      - release (delete)";
    items[3] = "Cancel";
    int sel = 0;
    for (;;) {
        plat_clear();
        linef(C_TITLE, "%s: %s", SIDE_NAME[a->focus], lab);
        put_line("", C_NORMAL);
        for (int i = 0; i < 4; i++) linef(i == sel ? C_HILITE : C_NORMAL, " %s", items[i]);
        put_line("", C_NORMAL);
        put_line("X Select   O Cancel", C_DIM);
        Btn b = plat_wait_button();
        if (b == B_UP)   sel = (sel + 3) % 4;
        if (b == B_DOWN) sel = (sel + 1) % 4;
        if (b == B_CIRCLE || b == B_QUIT) return ACT_NONE;
        if (b == B_CROSS) return sel == 3 ? ACT_NONE : sel + 1;
    }
}

static void transfer(App *a, int copy) {
    int s = a->focus, d = 1 - s;
    uint8_t *sm = side_mons(a, s), *dm = side_mons(a, d);
    int si = a->box[s] * G3_BOX_SLOTS + a->cur[s];
    if (!g3_mon_occupied(sm + si * G3_MON_SIZE)) { set_msg(a, "That slot is empty."); return; }
    int db, ds;
    if (g3_find_empty(dm, a->box[d], &db, &ds) != 0) { set_msg(a, "%s is full.", SIDE_NAME[d]); return; }
    char lab[48];
    mon_label(sm + si * G3_MON_SIZE, lab, sizeof lab);
    if (g3_move(sm, si, dm, db * G3_BOX_SLOTS + ds, copy) != 0) { set_msg(a, "Transfer failed."); return; }
    a->dirty[d] = a->gain[d] = 1;
    if (!copy) a->dirty[s] = 1;
    const char *verb = s == 0 ? (copy ? "Stored" : "Withdrew") : (copy ? "Copied" : "Deposited");
    set_msg(a, "%s %s -> box %d slot %d. Press TRI to save.", verb, lab, db + 1, ds + 1);
}

static void drop_mon(App *a) {
    int s = a->focus;
    uint8_t *sm = side_mons(a, s);
    int si = a->box[s] * G3_BOX_SLOTS + a->cur[s];
    if (!g3_mon_occupied(sm + si * G3_MON_SIZE)) { set_msg(a, "That slot is empty."); return; }
    char lab[48], q[96];
    mon_label(sm + si * G3_MON_SIZE, lab, sizeof lab);
    snprintf(q, sizeof q, "Release %s from %s?", lab, SIDE_NAME[s]);
    if (!ask_yes_no("DROP Pokemon", q)) { set_msg(a, "Cancelled."); return; }
    g3_clear(sm, si);
    a->dirty[s] = 1;
    set_msg(a, "Released %s. Press TRI to save.", lab);
}

static int write_side(App *a, int s) {
    const char *path = s == 0 ? a->path : plat_storage_path();
    char bak[300];
    if (!a->backed_up[s]) {
        int rc = file_backup(path, bak, sizeof bak);
        if (rc < 0) { set_msg(a, "Backup of %s failed - NOT saved.", SIDE_NAME[s]); return -1; }
        a->backed_up[s] = 1;
    }
    int rc;
    if (s == 0) { g3_commit(&a->sv); rc = file_write_verified(a->path, a->sv.data, a->sv.size); }
    else        { rc = store_save(&a->st, path); }
    if (rc != 0) { set_msg(a, "WRITE FAILED for %s! Restore from .bak", SIDE_NAME[s]); return -1; }
    a->dirty[s] = a->gain[s] = 0;
    return 0;
}

static int do_save(App *a) {
    if (!a->dirty[0] && !a->dirty[1]) { set_msg(a, "Nothing to save."); return 0; }
    /* Write the side that GAINED a Pokemon first, so a failure can never lose one. */
    int first = (a->gain[0] && !a->gain[1]) ? 0 : 1;
    for (int k = 0; k < 2; k++) {
        int s = k == 0 ? first : 1 - first;
        if (a->dirty[s] && write_side(a, s) != 0) return -1;
    }
    set_msg(a, "Saved. Backups: *.bak next to each file.");
    return 0;
}

/* returns 1 = quit app, 0 = back to file list */
static int main_screen(App *a) {
    for (;;) {
        draw_main(a);
        Btn b = plat_wait_button();
        a->msg[0] = 0;
        int s = a->focus;
        switch (b) {
        case B_QUIT: return 1;
        case B_UP:    a->cur[s] = (a->cur[s] + G3_BOX_SLOTS - 1) % G3_BOX_SLOTS; break;
        case B_DOWN:  a->cur[s] = (a->cur[s] + 1) % G3_BOX_SLOTS; break;
        case B_LEFT:  a->focus = 0; break;
        case B_RIGHT: a->focus = 1; break;
        case B_L:     a->box[s] = (a->box[s] + G3_NUM_BOXES - 1) % G3_NUM_BOXES; break;
        case B_R:     a->box[s] = (a->box[s] + 1) % G3_NUM_BOXES; break;
        case B_SQUARE: transfer(a, 0); break;
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
                plat_clear();
                linef(C_TITLE, "Unsaved changes");
                put_line("", C_NORMAL);
                put_line("X = Save and quit", C_NORMAL);
                put_line("O = Discard and quit", C_NORMAL);
                put_line("TRI = Keep editing", C_NORMAL);
                for (;;) {
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
    return l > 4 && n[l-4] == '.' && (n[l-3] | 32) == 's' && (n[l-2] | 32) == 'a' && (n[l-1] | 32) == 'v';
}

static int contains_leaf(const char *s) {
    for (; *s; s++)
        if ((s[0] | 32) == 'l' && (s[1] | 32) == 'e' && (s[2] | 32) == 'a' && (s[3] | 32) == 'f') return 1;
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
    plat_clear();
    put_line("Scanning for Pokemon Leaf Green .sav files...", C_TITLE);
    g_nfound = 0; g_visited = 0;
    scan_dir(plat_scan_root(), 0);
}

static const char *base_name(const char *p) {
    const char *b = p;
    for (; *p; p++) if (*p == '/' || *p == ':') b = p + 1;
    return b;
}

/* returns index into g_found, -1 quit, -2 rescan */
static int select_file(void) {
    int show_all = 0, sel = 0, top = 0;
    for (;;) {
        int idx[MAX_FOUND], n = 0;
        for (int i = 0; i < g_nfound; i++) if (show_all || g_found[i].leaf) idx[n++] = i;
        if (sel >= n) sel = n ? n - 1 : 0;
        plat_clear();
        linef(C_TITLE, "PKStorage - choose Leaf Green save (%s)", show_all ? "all .sav" : "names with 'Leaf'");
        put_line("", C_NORMAL);
        if (n == 0) {
            put_line("No matching .sav files found on the memory stick.", C_WARN);
            put_line("Put a copy of your save there (128KB, name containing 'Leaf')", C_NORMAL);
            put_line("or press SQ to list every .sav file.", C_NORMAL);
        }
        int rows = 26;
        if (sel < top) top = sel;
        if (sel >= top + rows) top = sel - rows + 1;
        for (int i = top; i < n && i < top + rows; i++) {
            const char *p = g_found[idx[i]].path;
            size_t l = strlen(p);
            char line[COLS + 1];
            if (l > COLS - 2) snprintf(line, sizeof line, "%c...%s", i == sel ? '>' : ' ', p + l - (COLS - 6));
            else snprintf(line, sizeof line, "%c%s", i == sel ? '>' : ' ', p);
            put_line(line, i == sel ? C_HILITE : C_NORMAL);
        }
        put_line("", C_NORMAL);
        put_line("X Open   SQ Toggle all/Leaf   TRI Rescan   START Quit", C_DIM);
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
    plat_clear();
    linef(C_WARN, "%s", l1);
    put_line("", C_NORMAL);
    put_line(l2, C_NORMAL);
    put_line("", C_NORMAL);
    put_line("Press any button.", C_DIM);
    plat_wait_button();
}

/* --------------------------------------------------------------------- run */
int app_run(void) {
    App *a = (App *)calloc(1, sizeof *a);
    if (!a) return 1;
    for (;;) {
        scan_all();
        int pick;
        for (;;) {
            pick = select_file();
            if (pick != -2) break;
            scan_all();
        }
        if (pick < 0) break;

        memset(a, 0, sizeof *a);
        snprintf(a->path, sizeof a->path, "%s", g_found[pick].path);
        snprintf(a->fname, sizeof a->fname, "%.63s", base_name(a->path));

        char err[160] = "";
        size_t size = 0;
        uint8_t *data = file_read_all(a->path, &size);
        if (!data) { show_error("Cannot read save file", a->path); continue; }
        if (g3_load(&a->sv, data, size, err, sizeof err) != 0) {
            free(data);
            show_error("Not a usable Gen 3 save", err);
            continue;
        }
        int sr = store_load(&a->st, plat_storage_path(), err, sizeof err);
        if (sr < 0) {
            g3_free(&a->sv);
            show_error("Storage file problem (nothing was changed)", err);
            continue;
        }
        if (sr == 1) set_msg(a, "New storage file will be created when you save.");
        int quit = main_screen(a);
        g3_free(&a->sv);
        if (quit) break;
    }
    free(a);
    return 0;
}
