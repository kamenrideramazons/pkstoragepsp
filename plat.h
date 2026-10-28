/* Thin platform layer: implemented by plat_psp.c (PSP) and plat_pc.c (keyboard test build). */
#ifndef PLAT_H
#define PLAT_H

typedef enum {
    B_NONE = 0, B_UP, B_DOWN, B_LEFT, B_RIGHT, B_CROSS, B_CIRCLE,
    B_TRIANGLE, B_SQUARE, B_L, B_R, B_START, B_QUIT
} Btn;

enum { C_NORMAL = 0, C_HILITE = 1, C_DIM = 2, C_TITLE = 3, C_WARN = 4 };

void plat_clear(void);
void plat_puts(const char *s);
void plat_color(int c);
Btn  plat_wait_button(void);

/* Calls cb for each entry in dir (never "." or ".."). cb returns nonzero to stop. */
typedef int (*plat_dir_cb)(const char *name, int is_dir, long size, void *ud);
void plat_walk_dir(const char *dir, plat_dir_cb cb, void *ud);

const char *plat_scan_root(void);      /* where scanning starts, e.g. "ms0:/" */
const char *plat_storage_path(void);   /* where PKSTORAGE.bin lives */
void plat_exit(void);
#endif
