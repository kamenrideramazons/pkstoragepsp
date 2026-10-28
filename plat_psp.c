/* PSP (PSPSDK) platform layer: debug text screen + controller + memory stick access. */
#include "plat.h"
#include "app.h"
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspiofilemgr.h>
#include <string.h>
#include <stdio.h>

PSP_MODULE_INFO("PKStorage", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

static char g_storage[256] = "ms0:/PSP/GAME/PKStorage/PKSTORAGE.bin";

static int exit_callback(int a, int b, void *c) { (void)a; (void)b; (void)c; sceKernelExitGame(); return 0; }
static int callback_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    int cb = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cb);
    sceKernelSleepThreadCB();
    return 0;
}
static void setup_callbacks(void) {
    int t = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (t >= 0) sceKernelStartThread(t, 0, 0);
}

void plat_clear(void) { pspDebugScreenClear(); pspDebugScreenSetXY(0, 0); }
void plat_puts(const char *s) { pspDebugScreenPuts(s); }

void plat_color(int c) {
    u32 fg = 0xFFFFFFFF, bg = 0xFF000000;
    switch (c) {
    case C_HILITE: fg = 0xFF000000; bg = 0xFF00FFFF; break;  /* black on yellow */
    case C_DIM:    fg = 0xFFAAAAAA; break;
    case C_TITLE:  fg = 0xFFFFFF00; break;                   /* cyan */
    case C_WARN:   fg = 0xFF40A0FF; break;                   /* orange */
    default: break;
    }
    pspDebugScreenSetBackColor(bg);
    pspDebugScreenSetTextColor(fg);
}

static const struct { unsigned mask; Btn btn; int repeat; } MAP[] = {
    { PSP_CTRL_UP, B_UP, 1 },         { PSP_CTRL_DOWN, B_DOWN, 1 },
    { PSP_CTRL_LEFT, B_LEFT, 0 },     { PSP_CTRL_RIGHT, B_RIGHT, 0 },
    { PSP_CTRL_CROSS, B_CROSS, 0 },   { PSP_CTRL_CIRCLE, B_CIRCLE, 0 },
    { PSP_CTRL_TRIANGLE, B_TRIANGLE, 0 }, { PSP_CTRL_SQUARE, B_SQUARE, 0 },
    { PSP_CTRL_LTRIGGER, B_L, 1 },    { PSP_CTRL_RTRIGGER, B_R, 1 },
    { PSP_CTRL_START, B_START, 0 },
};

Btn plat_wait_button(void) {
    static unsigned prev = 0xFFFFFFFF;    /* buttons already held are ignored until released */
    static int held = 0;
    SceCtrlData pad;
    const unsigned n = sizeof MAP / sizeof MAP[0];
    for (;;) {
        sceDisplayWaitVblankStart();
        sceCtrlPeekBufferPositive(&pad, 1);
        unsigned now = pad.Buttons, fresh = now & ~prev;
        for (unsigned i = 0; i < n; i++)
            if (fresh & MAP[i].mask) { prev = now; held = 0; return MAP[i].btn; }
        int rep = 0;                       /* auto-repeat for d-pad up/down and L/R */
        for (unsigned i = 0; i < n; i++) {
            if (MAP[i].repeat && (now & MAP[i].mask)) {
                rep = 1;
                if (++held > 18 && (held % 4) == 0) { prev = now; return MAP[i].btn; }
                break;
            }
        }
        if (!rep) held = 0;
        prev = now;
    }
}

void plat_walk_dir(const char *dir, plat_dir_cb cb, void *ud) {
    SceUID d = sceIoDopen(dir);
    if (d < 0) return;
    SceIoDirent ent;
    memset(&ent, 0, sizeof ent);
    while (sceIoDread(d, &ent) > 0) {
        if (strcmp(ent.d_name, ".") && strcmp(ent.d_name, "..")) {
            if (cb(ent.d_name, FIO_S_ISDIR(ent.d_stat.st_mode) ? 1 : 0, (long)ent.d_stat.st_size, ud)) break;
        }
        memset(&ent, 0, sizeof ent);
    }
    sceIoDclose(d);
}

const char *plat_scan_root(void) { return "ms0:/"; }
const char *plat_storage_path(void) { return g_storage; }
void plat_exit(void) { sceKernelExitGame(); }

int main(int argc, char *argv[]) {
    setup_callbacks();
    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    /* keep PKSTORAGE.bin next to the EBOOT */
    if (argc > 0 && argv[0]) {
        char *slash = strrchr(argv[0], '/');
        if (slash) {
            size_t n = (size_t)(slash - argv[0]) + 1;
            if (n + 16 < sizeof g_storage) {
                memcpy(g_storage, argv[0], n);
                strcpy(g_storage + n, "PKSTORAGE.bin");
            }
        }
    }
    app_run();
    sceKernelExitGame();
    return 0;
}
