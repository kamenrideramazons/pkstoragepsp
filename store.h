/* PKSTORAGE.bin: 'PKS1', u32 slot count, then 420 x 80-byte box Pokemon. */
#ifndef STORE_H
#define STORE_H
#include "gen3.h"
#define ST_FILE_SIZE (8 + G3_TOTAL_SLOTS * G3_MON_SIZE)
typedef struct { uint8_t mons[G3_TOTAL_SLOTS * G3_MON_SIZE]; } Store;
void store_init(Store *st);
/* 0 loaded, 1 file missing (empty store), -1 corrupt/unreadable (do NOT overwrite) */
int  store_load(Store *st, const char *path, char *err, size_t errlen);
int  store_save(const Store *st, const char *path);
#endif
