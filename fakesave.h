#ifndef FAKESAVE_H
#define FAKESAVE_H
#include <stdint.h>
#define FAKE_SAVE_SIZE 0x20000
void fake_make_mon(uint8_t *mon, uint32_t pers, uint32_t otid, int species, const char *nick, int egg);
void fake_build_save(uint8_t *buf);   /* synthetic FRLG-layout save; slot B (counter 6) is newest */
#endif
