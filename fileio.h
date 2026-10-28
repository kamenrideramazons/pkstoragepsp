#ifndef FILEIO_H
#define FILEIO_H
#include <stdint.h>
#include <stddef.h>
uint8_t *file_read_all(const char *path, size_t *size);   /* malloc'd, NULL on error */
int  file_exists(const char *path);
int  file_write_verified(const char *path, const uint8_t *data, size_t n); /* 0 ok */
/* Copies path to the first free path.bak, path.bak2, ... 0 ok, 1 nothing to back up, -1 error */
int  file_backup(const char *path, char *out, size_t outn);
#endif
