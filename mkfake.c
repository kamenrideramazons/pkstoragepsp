#include "fakesave.h"
#include <stdio.h>
int main(int argc, char **argv) {
    static uint8_t buf[FAKE_SAVE_SIZE];
    if (argc < 2) { fprintf(stderr, "usage: mkfake out.sav\n"); return 2; }
    fake_build_save(buf);
    FILE *f = fopen(argv[1], "wb");
    if (!f) return 1;
    fwrite(buf, 1, sizeof buf, f);
    fclose(f);
    return 0;
}
