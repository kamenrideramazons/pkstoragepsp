/* PC platform stub – minimal so the core logic still compiles/tests.
   Full graphical PC build is not the focus; use the PSP build for the real UI. */
#include "plat.h"
#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

static char g_storage[256] = "./PKSTORAGE.bin";

void plat_init(void) {}
void plat_shutdown(void) {}
void plat_begin_frame(void) {}
void plat_end_frame(void) {}
void plat_clear(uint32_t color) { (void)color; printf("\033[2J\033[H"); }
void plat_rect(int x, int y, int w, int h, uint32_t color) {
    (void)x; (void)y; (void)w; (void)h; (void)color;
}
void plat_rect_border(int x, int y, int w, int h, uint32_t color, int t) {
    (void)x; (void)y; (void)w; (void)h; (void)color; (void)t;
}
void plat_text(int x, int y, const char *s, uint32_t color) {
    (void)x; (void)y; (void)color;
    if (s) puts(s);
}
void plat_text_centered(int x, int y, int w, const char *s, uint32_t color) {
    (void)x; (void)y; (void)w; (void)color;
    if (s) puts(s);
}
void plat_sprite(int x, int y, int national_dex) {
    (void)x; (void)y; (void)national_dex;
}

Btn plat_wait_button(void) {
    int c = getchar();
    switch (c) {
    case 'w': case 'W': return B_UP;
    case 's': case 'S': return B_DOWN;
    case 'a': case 'A': return B_LEFT;
    case 'd': case 'D': return B_RIGHT;
    case 'z': case 'Z': return B_CROSS;
    case 'x': case 'X': return B_CIRCLE;
    case 't': case 'T': return B_TRIANGLE;
    case 'q': case 'Q': return B_SQUARE;
    case 'l': case 'L': return B_L;
    case 'r': case 'R': return B_R;
    case 'p': case 'P': return B_START;
    case 27:            return B_QUIT;
    default:            return B_NONE;
    }
}

void plat_walk_dir(const char *dir, plat_dir_cb cb, void *ud) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        char full[512];
        snprintf(full, sizeof full, "%s/%s", dir, ent->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (cb(ent->d_name, S_ISDIR(st.st_mode) ? 1 : 0, (long)st.st_size, ud)) break;
    }
    closedir(d);
}

const char *plat_scan_root(void) { return "."; }
const char *plat_storage_path(void) { return g_storage; }
void plat_exit(void) { exit(0); }

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    plat_init();
    app_run();
    plat_shutdown();
    return 0;
}
