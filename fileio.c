#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t *file_read_all(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0 || n > (8L << 20)) { fclose(f); return NULL; }
    rewind(f);
    uint8_t *b = (uint8_t *)malloc(n ? (size_t)n : 1);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *size = (size_t)n;
    return b;
}

int file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

int file_write_verified(const char *path, const uint8_t *data, size_t n) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    if (fwrite(data, 1, n, f) != n) { fclose(f); return -1; }
    if (fclose(f) != 0) return -1;
    size_t rn = 0;
    uint8_t *back = file_read_all(path, &rn);
    int ok = back && rn == n && memcmp(back, data, n) == 0;
    free(back);
    return ok ? 0 : -1;
}

int file_backup(const char *path, char *out, size_t outn) {
    size_t n = 0;
    uint8_t *d = file_read_all(path, &n);
    if (!d) return 1;
    int rc = -1;
    for (int i = 1; i < 100; i++) {
        if (i == 1) snprintf(out, outn, "%s.bak", path);
        else        snprintf(out, outn, "%s.bak%d", path, i);
        if (file_exists(out)) continue;
        rc = file_write_verified(out, d, n) == 0 ? 0 : -1;
        break;
    }
    free(d);
    return rc;
}
