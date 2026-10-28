#include "store.h"
#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void store_init(Store *st) { memset(st, 0, sizeof *st); }

int store_load(Store *st, const char *path, char *err, size_t en) {
    store_init(st);
    if (!file_exists(path)) return 1;
    size_t n = 0;
    uint8_t *d = file_read_all(path, &n);
    if (!d) { snprintf(err, en, "Cannot read %s", path); return -1; }
    int ok = n == ST_FILE_SIZE && memcmp(d, "PKS1", 4) == 0 &&
             d[4] == (G3_TOTAL_SLOTS & 0xFF) && d[5] == (G3_TOTAL_SLOTS >> 8) && d[6] == 0 && d[7] == 0;
    if (!ok) {
        free(d);
        snprintf(err, en, "Storage file is invalid or from another version.");
        return -1;
    }
    memcpy(st->mons, d + 8, sizeof st->mons);
    free(d);
    return 0;
}

int store_save(const Store *st, const char *path) {
    uint8_t *buf = (uint8_t *)malloc(ST_FILE_SIZE);
    if (!buf) return -1;
    memcpy(buf, "PKS1", 4);
    buf[4] = G3_TOTAL_SLOTS & 0xFF; buf[5] = G3_TOTAL_SLOTS >> 8; buf[6] = buf[7] = 0;
    memcpy(buf + 8, st->mons, sizeof st->mons);
    int rc = file_write_verified(path, buf, ST_FILE_SIZE);
    free(buf);
    return rc;
}
