#include "gen3.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void wr16(uint8_t *p, uint16_t v) { p[0] = v & 0xFF; p[1] = (uint8_t)(v >> 8); }

uint16_t g3_checksum(const uint8_t *d, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i + 3 < len; i += 4) sum += rd32(d + i);
    return (uint16_t)(sum + (sum >> 16));
}

static size_t pc_sector_len(int id) { return id == 13 ? 0x7D0 : 0xF80; }

static int check_slot(const uint8_t *data, uint32_t off, int pos[G3_NUM_SECTORS],
                      uint32_t *counter, char *why, size_t whyn) {
    int seen[G3_NUM_SECTORS] = {0};
    uint32_t ctr = 0;
    for (int p = 0; p < G3_NUM_SECTORS; p++) {
        const uint8_t *sec = data + off + (size_t)p * G3_SECTOR_SIZE;
        if (rd32(sec + 0xFF8) != G3_SIGNATURE) {
            snprintf(why, whyn, "sector %d has no Gen 3 signature", p); return -1;
        }
        unsigned id = rd16(sec + 0xFF4);
        if (id >= G3_NUM_SECTORS || seen[id]) {
            snprintf(why, whyn, "bad or duplicate section id"); return -1;
        }
        seen[id] = 1; pos[id] = p;
        uint32_t c = rd32(sec + 0xFFC);
        if (p == 0) ctr = c;
        else if (c != ctr) { snprintf(why, whyn, "save counter mismatch (interrupted save?)"); return -1; }
    }
    for (int id = G3_PC_FIRST_SECTOR; id < G3_NUM_SECTORS; id++) {
        const uint8_t *sec = data + off + (size_t)pos[id] * G3_SECTOR_SIZE;
        if (rd16(sec + 0xFF6) != g3_checksum(sec, pc_sector_len(id))) {
            snprintf(why, whyn, "checksum mismatch in section %d", id); return -1;
        }
    }
    *counter = ctr;
    return 0;
}

int g3_load(G3Save *s, uint8_t *data, size_t size, char *err, size_t en) {
    if (size < G3_SAVE_SIZE_MIN) {
        snprintf(err, en, "File too small (%lu bytes). Need a 128KB GBA save.", (unsigned long)size);
        return -1;
    }
    int posA[G3_NUM_SECTORS] = {0}, posB[G3_NUM_SECTORS] = {0};
    uint32_t cA = 0, cB = 0;
    char whyA[64] = "", whyB[64] = "";
    int okA = check_slot(data, 0, posA, &cA, whyA, sizeof whyA) == 0;
    int okB = check_slot(data, G3_SLOT_SIZE, posB, &cB, whyB, sizeof whyB) == 0;
    if (!okA && !okB) {
        snprintf(err, en, "Not a valid Gen 3 save. A: %s. B: %s.", whyA, whyB);
        return -1;
    }
    int useB = (okA && okB) ? (cB > cA) : okB;
    memset(s, 0, sizeof *s);
    s->data = data; s->size = size;
    s->slot_off = useB ? G3_SLOT_SIZE : 0;
    s->counter = useB ? cB : cA;
    memcpy(s->sec_pos, useB ? posB : posA, sizeof s->sec_pos);
    size_t off = 0;
    for (int id = G3_PC_FIRST_SECTOR; id < G3_NUM_SECTORS; id++) {
        const uint8_t *sec = data + s->slot_off + (size_t)s->sec_pos[id] * G3_SECTOR_SIZE;
        memcpy(s->pc + off, sec, pc_sector_len(id));
        off += pc_sector_len(id);
    }
    return 0;
}

void g3_free(G3Save *s) { free(s->data); memset(s, 0, sizeof *s); }

void g3_commit(G3Save *s) {
    size_t off = 0;
    for (int id = G3_PC_FIRST_SECTOR; id < G3_NUM_SECTORS; id++) {
        uint8_t *sec = s->data + s->slot_off + (size_t)s->sec_pos[id] * G3_SECTOR_SIZE;
        size_t len = pc_sector_len(id);
        memcpy(sec, s->pc + off, len);
        off += len;
        wr16(sec + 0xFF6, g3_checksum(sec, len));
    }
}

void g3_decode_text(const uint8_t *src, int max, char *out) {
    int n = 0;
    for (int i = 0; i < max; i++) {
        uint8_t c = src[i];
        char o;
        if (c == 0xFF) break;
        if (c >= 0xBB && c <= 0xD4) o = (char)('A' + (c - 0xBB));
        else if (c >= 0xD5 && c <= 0xEE) o = (char)('a' + (c - 0xD5));
        else if (c >= 0xA1 && c <= 0xAA) o = (char)('0' + (c - 0xA1));
        else switch (c) {
            case 0x00: o = ' '; break;
            case 0xAB: o = '!'; break;
            case 0xAC: o = '?'; break;
            case 0xAD: case 0xB0: o = '.'; break;
            case 0xAE: o = '-'; break;
            case 0xB1: case 0xB2: o = '"'; break;
            case 0xB3: case 0xB4: o = '\''; break;
            case 0xB5: o = 'M'; break;
            case 0xB6: o = 'F'; break;
            case 0xB8: o = ','; break;
            case 0xBA: o = '/'; break;
            default:   o = '?'; break;
        }
        out[n++] = o;
    }
    out[n] = 0;
}

void g3_box_name(const G3Save *s, int box, char out[10]) {
    g3_decode_text(s->pc + 4 + G3_TOTAL_SLOTS * G3_MON_SIZE + box * 9, 8, out);
}

/* ---- Pokemon ---- */
static const char *const ORDERS[24] = {
    "GAEM","GAME","GEAM","GEMA","GMAE","GMEA","AGEM","AGME","AEGM","AEMG","AMGE","AMEG",
    "EGAM","EGMA","EAGM","EAMG","EMGA","EMAG","MGAE","MGEA","MAGE","MAEG","MEGA","MEAG"
};

static void decrypt(const uint8_t *mon, uint8_t out[48]) {
    uint32_t key = rd32(mon) ^ rd32(mon + 4);
    for (int i = 0; i < 12; i++) {
        uint32_t v = rd32(mon + 0x20 + 4 * i) ^ key;
        out[4*i] = v & 0xFF; out[4*i+1] = (v >> 8) & 0xFF;
        out[4*i+2] = (v >> 16) & 0xFF; out[4*i+3] = (v >> 24) & 0xFF;
    }
}

int g3_mon_occupied(const uint8_t *mon) {
    return (mon[0x13] & 0x02) || rd32(mon) != 0 || rd32(mon + 4) != 0;
}

int g3_mon_is_egg(const uint8_t *mon) { return (mon[0x13] & 0x04) != 0; }

int g3_mon_species(const uint8_t *mon) {
    if (!g3_mon_occupied(mon)) return 0;
    uint8_t d[48];
    decrypt(mon, d);
    const char *o = ORDERS[rd32(mon) % 24];
    int p = 0;
    while (o[p] != 'G') p++;
    return rd16(d + 12 * p);
}

int g3_mon_checksum_ok(const uint8_t *mon) {
    uint8_t d[48];
    decrypt(mon, d);
    uint16_t sum = 0;
    for (int i = 0; i < 24; i++) sum = (uint16_t)(sum + rd16(d + 2 * i));
    return sum == rd16(mon + 0x1C);
}

void g3_mon_nickname(const uint8_t *mon, char out[11]) { g3_decode_text(mon + 8, 10, out); }

int g3_species_national(int idx) {
    if (idx >= 1 && idx <= 251) return idx;
    if (idx >= 277 && idx <= 411) return idx - 25;
    return 0;
}

/* ---- box ops ---- */
int g3_find_empty(const uint8_t *mons, int start_box, int *box, int *slot) {
    for (int b = 0; b < G3_NUM_BOXES; b++) {
        int bx = (start_box + b) % G3_NUM_BOXES;
        for (int sl = 0; sl < G3_BOX_SLOTS; sl++) {
            if (!g3_mon_occupied(mons + (bx * G3_BOX_SLOTS + sl) * G3_MON_SIZE)) {
                *box = bx; *slot = sl; return 0;
            }
        }
    }
    return -1;
}

int g3_move(uint8_t *src, int si, uint8_t *dst, int di, int copy) {
    uint8_t *a = src + si * G3_MON_SIZE, *b = dst + di * G3_MON_SIZE;
    if (!g3_mon_occupied(a) || g3_mon_occupied(b)) return -1;
    memcpy(b, a, G3_MON_SIZE);
    if (!copy) memset(a, 0, G3_MON_SIZE);
    return 0;
}

void g3_clear(uint8_t *mons, int idx) { memset(mons + idx * G3_MON_SIZE, 0, G3_MON_SIZE); }
