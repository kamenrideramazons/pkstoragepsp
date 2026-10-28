/* Gen 3 (FireRed/LeafGreen) save + Pokemon box helpers. Portable C, no platform code. */
#ifndef GEN3_H
#define GEN3_H
#include <stdint.h>
#include <stddef.h>

#define G3_SAVE_SIZE_MIN   0x1C000u   /* 2 slots x 14 sectors x 0x1000 */
#define G3_SLOT_SIZE       0xE000u
#define G3_SECTOR_SIZE     0x1000u
#define G3_NUM_SECTORS     14
#define G3_SIGNATURE       0x08012025u
#define G3_MON_SIZE        80
#define G3_NUM_BOXES       14
#define G3_BOX_SLOTS       30
#define G3_TOTAL_SLOTS     (G3_NUM_BOXES * G3_BOX_SLOTS)
#define G3_PC_SIZE         33744      /* sectors 5..13 data, concatenated */
#define G3_PC_FIRST_SECTOR 5
#define G3_PC_SECTORS      9

typedef struct {
    uint8_t *data;              /* whole .sav file (owned) */
    size_t   size;
    uint32_t slot_off;          /* 0 = slot A, G3_SLOT_SIZE = slot B */
    uint32_t counter;           /* save counter of the active slot */
    int      sec_pos[G3_NUM_SECTORS]; /* physical position of section id */
    uint8_t  pc[G3_PC_SIZE];    /* working copy of the PC storage buffer */
} G3Save;

/* Takes ownership of `data` only on success (return 0). */
int  g3_load(G3Save *s, uint8_t *data, size_t size, char *err, size_t errlen);
void g3_free(G3Save *s);
/* Write working PC buffer back into s->data and fix section checksums. */
void g3_commit(G3Save *s);

uint16_t g3_checksum(const uint8_t *d, size_t len);

static inline uint8_t *g3_save_mons(G3Save *s) { return s->pc + 4; }
void g3_box_name(const G3Save *s, int box, char out[10]);

/* --- single Pokemon (80-byte box format) --- */
int  g3_mon_occupied(const uint8_t *mon);
int  g3_mon_species(const uint8_t *mon);          /* internal index, 0 if none */
int  g3_mon_is_egg(const uint8_t *mon);
int  g3_mon_checksum_ok(const uint8_t *mon);
void g3_mon_nickname(const uint8_t *mon, char out[11]);
int  g3_species_national(int internal_index);     /* 0 if invalid */
const char *g3_species_name(int national);        /* "???" if invalid */

/* --- box slot operations on a 420-slot array (80 bytes each) --- */
int  g3_find_empty(const uint8_t *mons, int start_box, int *box, int *slot);
int  g3_move(uint8_t *src_mons, int si, uint8_t *dst_mons, int di, int copy);
void g3_clear(uint8_t *mons, int idx);

void g3_decode_text(const uint8_t *src, int max, char *out);
#endif
