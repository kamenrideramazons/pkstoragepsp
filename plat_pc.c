/* Keyboard build for testing the menu logic on a PC.
 * keys: w/s/a/d = up/down/left/right, z = X, x = O, t = triangle, q = square, l/r = L/R, p = START
 * PK_ROOT = folder to scan (default "."), PK_STORAGE = storage file (default ./PKSTORAGE.bin) */
#include "plat.h"
#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

void plat_clear(void) { puts("\n==================== screen ===================="); }
void plat_puts(const char *s) { fputs(s, stdout); }
void plat_color(int c) { (void)c; }

Btn plat_wait_button(void) {
    for (;;) {
        int c = getchar();
        if (c == EOF) return B_QUIT;
        switch (c) {
        case 'w': return B_UP;      case 's': return B_DOWN;
        case 'a': return B_LEFT;    case 'd': return B_RIGHT;
        case 'z': return B_CROSS;   case 'x': return B_CIRCLE;
        case 't': return B_TRIANGLE; case 'q': return B_SQUARE;
        case 'l': return B_L;       case 'r': return B_R;
        case 'p': return B_START;
        default: break;
        }
    }
}

void plat_walk_dir(const char *dir, plat_dir_cb cb, void *ud) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char full[1024];
        snprintf(full, sizeof full, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (cb(e->d_name, S_ISDIR(st.st_mode), (long)st.st_size, ud)) break;
    }
    closedir(d);
}

const char *plat_scan_root(void) { const char *r = getenv("PK_ROOT"); return r ? r : "."; }
const char *plat_storage_path(void) { const char *r = getenv("PK_STORAGE"); return r ? r : "./PKSTORAGE.bin"; }
void plat_exit(void) { exit(0); }

int main(void) { return app_run(); }
