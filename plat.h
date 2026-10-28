/* Thin platform layer: implemented by plat_psp.c (PSP) and plat_pc.c (keyboard test build). */
#ifndef PLAT_H
#define PLAT_H

#include <stdint.h>

/* ---------- input ---------- */
typedef enum {
    B_NONE = 0, B_UP, B_DOWN, B_LEFT, B_RIGHT, B_CROSS, B_CIRCLE,
    B_TRIANGLE, B_SQUARE, B_L, B_R, B_START, B_QUIT
} Btn;

/* ---------- colours (0xAABBGGRR for PSP GU) ---------- */
enum {
    C_BG         = 0xFF1A1A2E,   /* dark blue-black */
    C_PANEL      = 0xFF16213E,
    C_SLOT       = 0xFF0F3460,
    C_SLOT_H     = 0xFFE94560,   /* highlight / cursor */
    C_SLOT_EMPTY = 0xFF1F4068,
    C_TEXT       = 0xFFFFFFFF,
    C_TEXT_DIM   = 0xFFAAAAAA,
    C_TITLE      = 0xFF00D2FF,
    C_WARN       = 0xFF4CC9F0,
    C_ACCENT     = 0xFFFFD700,
    C_LEFT       = 0xFF4ADE80,   /* LeafGreen side accent */
    C_RIGHT      = 0xFF60A5FA    /* Storage side accent */
};

/* ---------- lifecycle ---------- */
void plat_init(void);
void plat_shutdown(void);
void plat_begin_frame(void);
void plat_end_frame(void);

/* ---------- drawing ---------- */
void plat_clear(uint32_t color);
void plat_rect(int x, int y, int w, int h, uint32_t color);
void plat_rect_border(int x, int y, int w, int h, uint32_t color, int thickness);
void plat_text(int x, int y, const char *s, uint32_t color);
void plat_text_centered(int x, int y, int w, const char *s, uint32_t color);

/* Sprite: national dex number (1-386). 0 = empty / no sprite.
   Draws a 32x32 sprite (or coloured placeholder if atlas missing). */
void plat_sprite(int x, int y, int national_dex);

/* ---------- input ---------- */
Btn  plat_wait_button(void);

/* ---------- filesystem ---------- */
typedef int (*plat_dir_cb)(const char *name, int is_dir, long size, void *ud);
void plat_walk_dir(const char *dir, plat_dir_cb cb, void *ud);
const char *plat_scan_root(void);
const char *plat_storage_path(void);
void plat_exit(void);

#endif
