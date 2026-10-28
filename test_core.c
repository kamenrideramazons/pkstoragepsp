#include "fakesave.h"
#include "gen3.h"
#include "store.h"
#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static uint8_t *dup_fake(void) { uint8_t *b = malloc(FAKE_SAVE_SIZE); fake_build_save(b); return b; }

int main(void) {
    char err[200];
    CHECK(!strcmp(g3_species_name(1), "Bulbasaur"));   CHECK(!strcmp(g3_species_name(25), "Pikachu"));
    CHECK(!strcmp(g3_species_name(83), "Farfetch'd")); CHECK(!strcmp(g3_species_name(100), "Voltorb"));
    CHECK(!strcmp(g3_species_name(129), "Magikarp"));  CHECK(!strcmp(g3_species_name(151), "Mew"));
    CHECK(!strcmp(g3_species_name(152), "Chikorita")); CHECK(!strcmp(g3_species_name(200), "Misdreavus"));
    CHECK(!strcmp(g3_species_name(249), "Lugia"));     CHECK(!strcmp(g3_species_name(251), "Celebi"));
    CHECK(!strcmp(g3_species_name(252), "Treecko"));   CHECK(!strcmp(g3_species_name(300), "Skitty"));
    CHECK(!strcmp(g3_species_name(350), "Milotic"));   CHECK(!strcmp(g3_species_name(386), "Deoxys"));
    CHECK(!strcmp(g3_species_name(0), "???"));         CHECK(!strcmp(g3_species_name(387), "???"));
    CHECK(g3_species_national(277) == 252); CHECK(g3_species_national(411) == 386);
    CHECK(g3_species_national(260) == 0);   CHECK(g3_species_national(412) == 0);

    uint8_t *orig = dup_fake();
    uint8_t *d = dup_fake();
    G3Save s;
    CHECK(g3_load(&s, d, FAKE_SAVE_SIZE, err, sizeof err) == 0);
    CHECK(s.slot_off == 0xE000 && s.counter == 6);
    uint8_t *m = g3_save_mons(&s);
    CHECK(g3_mon_occupied(m) && g3_species_national(g3_mon_species(m)) == 1);
    CHECK(g3_mon_checksum_ok(m));
    char nick[11]; g3_mon_nickname(m + 80, nick); CHECK(!strcmp(nick, "ZAPPY"));
    CHECK(g3_mon_is_egg(m + 5 * 80) && !g3_mon_is_egg(m));
    CHECK(g3_species_national(g3_mon_species(m + 33 * 80)) == 386);
    CHECK(g3_species_national(g3_mon_species(m + 5 * 80)) == 252);
    CHECK(!g3_mon_occupied(m + 2 * 80));
    char bn[10]; g3_box_name(&s, 0, bn); CHECK(!strcmp(bn, "BOX 1"));
    g3_box_name(&s, 13, bn); CHECK(!strcmp(bn, "BOX 14"));
    uint8_t bad[80]; memcpy(bad, m, 80); bad[0x30] ^= 0x55; CHECK(!g3_mon_checksum_ok(bad));

    static Store st; store_init(&st);
    CHECK(g3_move(m, 0, st.mons, 0, 0) == 0);
    CHECK(!g3_mon_occupied(m) && g3_mon_occupied(st.mons));
    CHECK(g3_move(m, 0, st.mons, 1, 0) != 0);
    CHECK(g3_move(m, 1, st.mons, 0, 0) != 0);
    CHECK(g3_move(m, 1, st.mons, 1, 1) == 0);
    CHECK(g3_mon_occupied(m + 80) && g3_mon_occupied(st.mons + 80));
    int bx, sl;
    CHECK(g3_find_empty(st.mons, 0, &bx, &sl) == 0 && bx == 0 && sl == 2);
    CHECK(g3_find_empty(st.mons, 5, &bx, &sl) == 0 && bx == 5 && sl == 0);
    static Store full; for (int i = 0; i < 420; i++) memset(full.mons + i * 80, 1, 80);
    CHECK(g3_find_empty(full.mons, 3, &bx, &sl) != 0);
    g3_move(st.mons, 0, m, 0, 0);

    g3_clear(m, 1);
    memcpy(m + 40 * 80, st.mons + 80, 80);
    g3_commit(&s);
    uint8_t *copy = malloc(FAKE_SAVE_SIZE); memcpy(copy, s.data, FAKE_SAVE_SIZE);
    G3Save s2;
    CHECK(g3_load(&s2, copy, FAKE_SAVE_SIZE, err, sizeof err) == 0);
    CHECK(s2.slot_off == 0xE000 && !memcmp(s2.pc, s.pc, G3_PC_SIZE));
    CHECK(!memcmp(s.data, orig, 0xE000));
    CHECK(!memcmp(s.data + 0x1C000, orig + 0x1C000, 0x4000));
    int same = 1;
    for (int p = 0; p < 14; p++) {
        int id = s.data[0xE000 + p * 0x1000 + 0xFF4];
        if (id < 5 && memcmp(s.data + 0xE000 + p * 0x1000, orig + 0xE000 + p * 0x1000, 0x1000)) same = 0;
    }
    CHECK(same);
    CHECK(memcmp(s.data, orig, FAKE_SAVE_SIZE) != 0);   /* the edit really changed the file */
    g3_free(&s2);

    uint8_t *o2 = dup_fake(); G3Save s3;
    CHECK(g3_load(&s3, o2, FAKE_SAVE_SIZE, err, sizeof err) == 0);
    g3_commit(&s3);
    CHECK(!memcmp(s3.data, orig, FAKE_SAVE_SIZE));      /* no-op commit == byte-identical */
    g3_free(&s3);

    uint8_t *c = dup_fake();
    for (int p = 0; p < 14; p++) if (c[0xE000 + p * 0x1000 + 0xFF4] == 6) c[0xE000 + p * 0x1000 + 100] ^= 1;
    G3Save s4;
    CHECK(g3_load(&s4, c, FAKE_SAVE_SIZE, err, sizeof err) == 0 && s4.slot_off == 0 && s4.counter == 5);
    g3_free(&s4);
    uint8_t *c2 = dup_fake(); G3Save s5;
    for (int p = 0; p < 14; p++) { c2[p * 0x1000 + 0x10] ^= 1; c2[0xE000 + p * 0x1000 + 0x10] ^= 1; }
    CHECK(g3_load(&s5, c2, FAKE_SAVE_SIZE, err, sizeof err) != 0 && strstr(err, "Not a valid"));
    free(c2);
    uint8_t *tiny = calloc(1, 0x10000);
    CHECK(g3_load(&s5, tiny, 0x10000, err, sizeof err) != 0); free(tiny);
    uint8_t *zero = calloc(1, 0x20000);
    CHECK(g3_load(&s5, zero, 0x20000, err, sizeof err) != 0); free(zero);
    g3_free(&s); free(orig);

    const char *sp = "/tmp/pk_test_store.bin";
    remove(sp);
    static Store a, b;
    CHECK(store_load(&a, sp, err, sizeof err) == 1);
    fake_make_mon(a.mons + 7 * 80, 0x1234, 0x5678, 133, "EEVEE", 0);
    CHECK(store_save(&a, sp) == 0);
    CHECK(store_load(&b, sp, err, sizeof err) == 0 && !memcmp(a.mons, b.mons, sizeof a.mons));
    FILE *f = fopen(sp, "r+b"); fputc('X', f); fclose(f);
    CHECK(store_load(&b, sp, err, sizeof err) == -1);
    remove(sp);

    const char *bp = "/tmp/pk_test_file.sav";
    char o[300];
    remove(bp); remove("/tmp/pk_test_file.sav.bak"); remove("/tmp/pk_test_file.sav.bak2");
    CHECK(file_backup(bp, o, sizeof o) == 1);
    CHECK(file_write_verified(bp, (const uint8_t *)"hello", 5) == 0);
    CHECK(file_backup(bp, o, sizeof o) == 0 && strstr(o, ".bak") && !strstr(o, ".bak2"));
    CHECK(file_backup(bp, o, sizeof o) == 0 && strstr(o, ".bak2"));
    remove(bp); remove("/tmp/pk_test_file.sav.bak"); remove("/tmp/pk_test_file.sav.bak2");

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
