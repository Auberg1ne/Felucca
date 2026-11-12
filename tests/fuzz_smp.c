/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Fuzz harness: random user sample slot headers (half of them valid, zones at the edges of the data) ->
 * smp_user_scan -> SAMPLE / GRAIN / SLICE notes and render. run_tests.sh runs it briefly under ASan / UBSan with a
 * fixed seed.   fuzz_smp [ITERATIONS [SEED]] */
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"                                /* hostsim, the stubs, RAM flash, reset() */
static uint32_t rs = 0x6A09E667u;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
int main(int argc, char **argv)
{
    static int32_t o[CTL * 2 + 16];
    uint32_t iters = argc > 1 ? (uint32_t)atoi(argv[1]) : 20000u, i, j, k, valid = 0, covered = 0;
    if (argc > 2) rs = (uint32_t)atoi(argv[2]) | 1u;
    reset();
    for (i = 0; i < iters; i++) {
        /* A new isolated boot image, not an in-place overwrite of flash still
         * referenced by old voices. Real C2 replacements retain the old extent. */
        for (j = 0; j < NTRK; j++) memset(trk[j].v, 0, sizeof trk[j].v);
        k = rnd() % SMP_USER_SLOTS;
        const uint32_t phys = SMP_USER_BASE + SMP_USER_SECTOR_SIZE;
        uint8_t *image = host_nor + phys;
        host_bounds(phys, SMP_USER_SIZE);
        /* One isolated boot image per iteration: no other slot may alias it. */
        for (j = 0; j < SMP_USER_SLOTS; j++) {
            smp_user_phys[j] = 0; smp_user_sectors[j] = 0;
            if (j != k) usr_nz[j] = 0;
        }
        smp_user_hdr_t *h = (smp_user_hdr_t *)image;
        for (j = 0; j < SMP_USER_SIZE; j += 1u + rnd() % 7u) image[j] = (uint8_t)rnd();
        if (rnd() % 8u) { h->magic = SMP_USER_MAGIC; h->version = 1; }
        h->nz = (uint8_t)(rnd() % 4u == 0 ? rnd() : 1u + rnd() % 16u);
        h->data_len = rnd() % 4u == 0 ? rnd() : rnd() % (SMP_USER_SIZE - SMP_USER_DATA + 1u);
        int good = rnd() % 2u;
        if (good) {
            h->magic = SMP_USER_MAGIC; h->version = 1;
            h->nz = (uint8_t)(1u + rnd() % 16u);
            h->data_len = 1024u + rnd() % (SMP_USER_SIZE - SMP_USER_DATA - 1024u);
        }
        for (j = 0; j < 16u; j++) {
            smp_zone_t *z = &h->zone[j];
            if (good || rnd() % 4u) {
                uint32_t maxb = h->data_len > SMP_USER_SIZE - SMP_USER_DATA ? SMP_USER_SIZE - SMP_USER_DATA : h->data_len;
                if (!maxb) maxb = 1; /* the fuzz generator itself must not divide by zero */
                uint32_t nmax = maxb * 2u < 8192u ? maxb * 2u : 8192u;
                z->n = 1u + rnd() % nmax;
                uint32_t bytes = z->n / 2u + (z->n & 1u);
                z->off = rnd() % (maxb - bytes + 1u);
                z->ls = rnd() % z->n; z->le = z->ls + rnd() % (z->n - z->ls);
                z->rate = good || rnd() % 16u ? 1u + rnd() % (4u << 16) : rnd();
                z->idx = (uint8_t)(good || rnd() % 16u ? rnd() % 89u : rnd());
                z->lo = 0; z->hi = 127;
                z->looped = (uint8_t)(rnd() % 2u); z->root16 = (int16_t)(rnd() % (127u * 16u + 1u)); z->pred = (int16_t)rnd();
            }
        }
        /* Explicit isolated metadata bindings, so H2 does not make every fuzz
         * iteration reject before H1/engine coverage. This harness loads an isolated
         * physical image, not an allocator; physical journal/ownership fuzzing
         * belongs to sample_delete_test.c and the real simulator tests. */
        if (h->data_len <= SMP_USER_SIZE - SMP_USER_DATA)
            h->crc = st_crc32(image + SMP_USER_DATA, h->data_len);
        smp_journal_state = SMP_JOURNAL_READY;
        smp_user_phys[k] = phys; smp_user_sectors[k] = SMP_USER_SECTORS_PER_LEGACY_SLOT;
        smp_user_deleted[k] = smp_user_unresolved[k] = 0;
        smp_user_header_crc[k] = st_crc32(h, sizeof *h); smp_user_header_bound[k] = 1;
        /* Exercise both cleared and stale metadata caches on the isolated
         * image. Production active-reader retention is tested on real NOR. */
        if (rnd() % 2u) {
            uint32_t q;
            for (q = 0; q < 16u; q++) usr_zone[k][q].n = 0;
            usr_nz[k] = 0;
            for (q = 0; q < 4u; q++) mix_block(o, CTL);
        }
        smp_user_scan(k);
        if (usr_nz[k]) { valid++; covered |= 1u << k; }
        for (j = 0; j < NTRK; j++) {                                 /* SAMPLE, GRAIN, SLICE on the slot */
            uint32_t e = (j % 3u == 0) ? 4u : (j % 3u == 1) ? 8u : 13u;
            set_engine_of(&trk[j], e);
            trk[j].p[P_E0] = (int16_t)(e == 13u ? slc_src_from_slot(k) : SMP_NSETS + k);
            trk[j].p[P_E1] = (int16_t)(rnd() % 128u); trk[j].p[P_E2] = (int16_t)(rnd() % 128u);
            trk_note_on(&trk[j], 24u + rnd() % 84u, 100);
        }
        for (j = 0; j < 24u; j++) { mix_block(o, CTL); fm1_ms++; ed_service(); host_drain(); }
        for (j = 0; j < NTRK; j++) trk_note_on(&trk[j], 24u + rnd() % 84u, 100);
        for (j = 0; j < 24u; j++) mix_block(o, CTL);
    }
    printf("fuzz_smp ok: %u iterations, %u valid slots, slot mask 0x%02x\n", iters, valid, covered);
    return iters >= 1000 && covered != 255u ? 1 : 0;
}
