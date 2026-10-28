/* checkfile <mode> <save> [storage]   exit 0 = pass */
#include "gen3.h"
#include "store.h"
#include "fileio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int sp(const uint8_t *mons, int idx) { return g3_species_national(g3_mon_species(mons + idx * 80)); }

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    const char *mode = argv[1];
    size_t n; char err[200];
    uint8_t *d = file_read_all(argv[2], &n);
    G3Save sv;
    if (!d || g3_load(&sv, d, n, err, sizeof err)) { printf("load failed: %s\n", err); return 1; }
    uint8_t *m = g3_save_mons(&sv);
    static Store st; int sr = -2;
    if (argc > 3) sr = store_load(&st, argv[3], err, sizeof err);
    int ok = 1;
#define EXPECT(c) do { if (!(c)) { printf("FAIL: %s (line %d)\n", #c, __LINE__); ok = 0; } } while (0)
    EXPECT(sv.slot_off == 0xE000);
    if (!strcmp(mode, "withdrawn")) {
        EXPECT(sr == 0);
        EXPECT(sp(m, 0) == 0); EXPECT(sp(m, 1) == 25); EXPECT(sp(m, 5) == 252); EXPECT(sp(m, 33) == 386);
        EXPECT(sp(st.mons, 0) == 1);
        EXPECT(sp(st.mons, 1) == 0);
    } else if (!strcmp(mode, "stored")) {
        EXPECT(sr == 0); EXPECT(sp(m, 0) == 1); EXPECT(sp(st.mons, 0) == 1);
    } else if (!strcmp(mode, "storage-empty")) {
        EXPECT(sr == 0);
        for (int i = 0; i < 420; i++) EXPECT(sp(st.mons, i) == 0);
        EXPECT(sp(m, 0) == 1);
    } else { printf("bad mode\n"); return 2; }
    g3_free(&sv);
    printf("%s: %s\n", mode, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
