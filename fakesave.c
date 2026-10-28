#include "fakesave.h"
#include "gen3.h"
#include <string.h>

static void w16(uint8_t *p, unsigned v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }
static void w32(uint8_t *p, uint32_t v) { w16(p, v & 0xFFFF); w16(p + 2, v >> 16); }
static unsigned r16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint32_t r32(const uint8_t *p) { return r16(p) | ((uint32_t)r16(p + 2) << 16); }

static const char *const ORD[24] = {
    "GAEM","GAME","GEAM","GEMA","GMAE","GMEA","AGEM","AGME","AEGM","AEMG","AMGE","AMEG",
    "EGAM","EGMA","EAGM","EAMG","EMGA","EMAG","MGAE","MGEA","MAGE","MAEG","MEGA","MEAG"
};

static void enc_text(const char *s, uint8_t *out, int len) {
    memset(out, 0xFF, len);
    for (int i = 0; s[i] && i < len; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') out[i] = 0xBB + (c - 'A');
        else if (c >= 'a' && c <= 'z') out[i] = 0xD5 + (c - 'a');
        else if (c >= '0' && c <= '9') out[i] = 0xA1 + (c - '0');
        else if (c == ' ') out[i] = 0x00;
        else out[i] = 0xAC;
    }
}

void fake_make_mon(uint8_t *mon, uint32_t pers, uint32_t otid, int species, const char *nick, int egg) {
    uint8_t plain[48] = {0};
    const char *o = ORD[pers % 24];
    int p = 0;
    while (o[p] != 'G') p++;
    w16(plain + 12 * p, species);
    w32(plain + 12 * p + 4, 1000);
    plain[12 * p + 9] = 70;
    uint16_t sum = 0;
    for (int i = 0; i < 24; i++) sum = (uint16_t)(sum + r16(plain + 2 * i));
    memset(mon, 0, 80);
    w32(mon, pers); w32(mon + 4, otid);
    enc_text(nick, mon + 8, 10);
    mon[0x12] = 2;
    mon[0x13] = 0x02 | (egg ? 0x04 : 0);
    enc_text("RED", mon + 0x14, 7);
    w16(mon + 0x1C, sum);
    uint32_t key = pers ^ otid;
    for (int i = 0; i < 12; i++) w32(mon + 0x20 + 4 * i, r32(plain + 4 * i) ^ key);
}

static size_t sec_len(int id) { return id == 13 ? 0x7D0 : id >= 5 ? 0xF80 : id == 0 ? 0xF24 : id == 4 ? 0xEE8 : 0xF80; }

static void build_slot(uint8_t *slot, uint32_t counter, int rot, const uint8_t *pc) {
    size_t pcoff = 0;
    for (int p = 0; p < 14; p++) {
        int id = (p + rot) % 14;
        uint8_t *sec = slot + (size_t)p * 0x1000;
        memset(sec, 0, 0x1000);
        if (id < 5) for (size_t i = 0; i < sec_len(id); i++) sec[i] = (uint8_t)(i * 31 + id + counter);
        w16(sec + 0xFF4, id);
        w32(sec + 0xFF8, 0x08012025);
        w32(sec + 0xFFC, counter);
    }
    for (int id = 5; id < 14; id++) {              /* PC buffer is stored in section-id order */
        for (int p = 0; p < 14; p++) {
            if ((p + rot) % 14 != id) continue;
            memcpy(slot + (size_t)p * 0x1000, pc + pcoff, sec_len(id));
            pcoff += sec_len(id);
        }
    }
    for (int p = 0; p < 14; p++) {
        uint8_t *sec = slot + (size_t)p * 0x1000;
        w16(sec + 0xFF6, g3_checksum(sec, sec_len(r16(sec + 0xFF4))));
    }
}

void fake_build_save(uint8_t *buf) {
    static uint8_t pcA[G3_PC_SIZE], pcB[G3_PC_SIZE];
    memset(buf, 0xFF, FAKE_SAVE_SIZE);
    memset(pcA, 0, sizeof pcA); memset(pcB, 0, sizeof pcB);
    for (int b = 0; b < 14; b++) {
        char nm[12];
        if (b < 9) { nm[0]='B'; nm[1]='O'; nm[2]='X'; nm[3]=' '; nm[4]=(char)('1'+b); nm[5]=0; }
        else       { nm[0]='B'; nm[1]='O'; nm[2]='X'; nm[3]=' '; nm[4]='1'; nm[5]=(char)('0'+(b-9)); nm[6]=0; }
        enc_text(nm, pcA + 4 + 33600 + b * 9, 9);
        enc_text(nm, pcB + 4 + 33600 + b * 9, 9);
    }
    fake_make_mon(pcA + 4, 0x11111111, 0x22222222, 16, "PIDGEY", 0);   /* slot A: older */
    uint8_t *m = pcB + 4;                                                /* slot B: newer */
    fake_make_mon(m + 0 * 80,  0x12345678, 0xA1B2C3D4, 1,   "BULBASAUR", 0);
    fake_make_mon(m + 1 * 80,  0x0BADF00D, 0xA1B2C3D4, 25,  "ZAPPY",     0);
    fake_make_mon(m + 5 * 80,  0x00C0FFEE, 0x13572468, 277, "TREECKO",   1);
    fake_make_mon(m + 33 * 80, 0x7777ABCD, 0x01020304, 411, "DEOXYS",    0);
    build_slot(buf, 5, 3, pcA);
    build_slot(buf + 0xE000, 6, 7, pcB);
}
