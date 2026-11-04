/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SAMPLE: IMA ADPCM sample sets. */
static const char *const N_ONOFF_E[] = {"OFF", "ON"};
/* IMA ADPCM sample sets (tools/gen_samples.py), native rates, zones across
 * the keyboard, loops with the ADPCM state stored at the loop start.
 * SET is P_E0 (EDIT 1, KNOB 1). SET 4 was PERC, the GM drum kit: retired, an alias of PIANO; its sounds
 * load as the DRUM engine (core.h drum_from_perc). */
typedef struct {
    uint32_t off, n, ls, le;     /* byte offset, samples, loop start / end (sample index) */
    uint32_t rate;               /* source rate / 44100, Q16 */
    int16_t root16;              /* root pitch in 1/16 semitone */
    int16_t pred;                /* ADPCM state at the loop start */
    uint8_t idx, lo, hi, looped;
} smp_zone_t;
typedef struct {
    const char *name;
    uint16_t z0, nz;
} smp_set_t;
#include "felucca_samples.h"
#if defined(SMP_PERC_SLOT) && SMP_PERC_SLOT != SMP_SET_PERC
#error "tools/gen_samples.py PERC_SLOT != core.h SMP_SET_PERC"
#endif

static const int16_t IMA_STEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
    4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767};
static const int8_t IMA_IDX[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
/* 2^(i/192), Q16: pitch ratios in 1/16 semitones, d16 >= -3072 (16 octaves down) */
static uint32_t pow2_q16(int32_t d16)
{
    uint32_t u = (uint32_t)(d16 + 192 * 16), oct = u / 192u;     /* no loop for negative d16 */
    uint32_t r = pitch_inc(1600 + u % 192u) / (pitch_inc(1600) >> 16);   /* 2^(d/192) via the pitch table */
    return oct >= 16u ? r << (oct - 16u) : r >> (16u - oct);
}

/* ---- user sample slots (loaded from the web editor into flash, see web/EDITOR_PROTOCOL.md)
 * 3 slots of 80 KiB at flash 0xA0000.. (Felucca data region), read through the plain XIP
 * window. Slot = header (magic, count, name, data length, CRC32) + up to 16 zones in the
 * smp_zone_t layout (off relative to the slot's data at +512) + IMA ADPCM data. */
#include "../hal/fm1_xip.h"   /* relative: hostsim includes this file too */
#include "sample_storage.h"
#include "sample_alloc.h"

static uint32_t st_crc32(const void *, uint32_t);
#define SMP_NALL (SMP_NSETS + SMP_USER_SLOTS)
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t nz, rsv;
    char name[8];
    uint32_t data_len, crc, rsv2[2];
    smp_zone_t zone[16];
} smp_user_hdr_t;                                   /* 32 + 16 x 28 = 480 B, data at +512 */
static smp_zone_t usr_zone[SMP_USER_LOGICAL_SLOTS][16] __attribute__((section(".pool")));     /* RAM copy, off rebased onto SMP_DATA */
static uint8_t usr_nz[SMP_USER_LOGICAL_SLOTS] __attribute__((section(".pool")));
static const char *const SMP_ALL_NAMES[SMP_NALL] = {SMP_SET_NAMES_INIT, "USR1", "USR2", "USR3"}; /* USR4-8 are appended when allocator support is enabled. */

/* Logical-to-physical map. Boot clears these defaults; only journal recovery
 * authorizes a mapping. Zero means unpublished. */
static uint32_t smp_user_phys[SMP_USER_LOGICAL_SLOTS] __attribute__((section(".pool"))) = {
    SMP_USER_BASE,
    SMP_USER_BASE + SMP_USER_SIZE,
    SMP_USER_BASE + 2u * SMP_USER_SIZE,
    0u, 0u, 0u, 0u, 0u
};

/* Dynamic allocation bookkeeping: 60 sectors in the user-sample pool. */
static uint8_t smp_user_sectors[SMP_USER_LOGICAL_SLOTS] __attribute__((section(".pool"))) = {
    SMP_USER_SECTORS_PER_LEGACY_SLOT,
    SMP_USER_SECTORS_PER_LEGACY_SLOT,
    SMP_USER_SECTORS_PER_LEGACY_SLOT,
    0u, 0u, 0u, 0u, 0u
};
/* Latest committed generation, including a replacement not yet running. */
static uint32_t smp_user_generation[SMP_USER_LOGICAL_SLOTS] __attribute__((section(".pool")));
/* Latest durable state (or presence of a deletion barrier in a quarantined
 * slot), distinct from the running map retained for readers. */
static uint8_t smp_user_deleted[SMP_USER_LOGICAL_SLOTS] __attribute__((section(".pool")));
/* Unresolved I/O or recovery conflicts must never use stale RAM generations. */
static uint8_t smp_user_unresolved[SMP_USER_LOGICAL_SLOTS] __attribute__((section(".pool")));
static uint8_t smp_user_io_blocked;
static uint8_t smp_journal_state;
static uint16_t smp_journal_next;
static uint8_t smp_user_has_records[SMP_USER_LOGICAL_SLOTS];
static uint32_t smp_user_oldest_generation[SMP_USER_LOGICAL_SLOTS];
static uint8_t smp_user_occupied[(SMP_USER_SECTOR_COUNT + 7u) / 8u] __attribute__((section(".pool")));
/* Temporary upload extents. These never change the active logical-slot map. */
static uint8_t smp_stage_start[SMP_USER_LOGICAL_SLOTS] __attribute__((section(".pool")));
static uint8_t smp_stage_count[SMP_USER_LOGICAL_SLOTS] __attribute__((section(".pool")));


/* Sector bitmap helpers. Sector indices are relative to SMP_USER_BASE. */
static int smp_alloc_test(uint32_t sector)
{
    if (sector >= SMP_USER_SECTOR_COUNT)
        return 1;
    return (smp_user_occupied[sector >> 3] >> (sector & 7u)) & 1u;
}

static void smp_alloc_mark(uint32_t sector)
{
    if (sector < SMP_USER_SECTOR_COUNT)
        smp_user_occupied[sector >> 3] |= (uint8_t)(1u << (sector & 7u));
}

static void smp_alloc_release(uint32_t sector)
{
    if (sector < SMP_USER_SECTOR_COUNT)
        smp_user_occupied[sector >> 3] &= (uint8_t)~(1u << (sector & 7u));
}

/* Reserve a complete contiguous extent, or leave the bitmap unchanged. */
static int smp_alloc_reserve(uint32_t start, uint32_t count)
{
    uint32_t i;
    if (!count || start >= SMP_USER_SECTOR_COUNT ||
        count > SMP_USER_SECTOR_COUNT - start)
        return -1;
    for (i = 0; i < count; i++)
        if (smp_alloc_test(start + i))
            return -1;
    for (i = 0; i < count; i++)
        smp_alloc_mark(start + i);
    return 0;
}

/* Rebuild the bitmap with the three original fixed slots pinned. */
static void smp_alloc_reserve_legacy(void)
{
    uint32_t k, sector;
    for (sector = 0; sector < SMP_USER_SECTOR_COUNT; sector++)
        smp_alloc_release(sector);
    for (k = 0; k < SMP_USER_LEGACY_SLOTS; k++)
        (void)smp_alloc_reserve(k * SMP_USER_SECTORS_PER_LEGACY_SLOT,
                                SMP_USER_SECTORS_PER_LEGACY_SLOT);
}

/* Validate the descriptor record CRC (commit marker is deliberately excluded). */
static int smp_alloc_desc_valid(const smp_alloc_desc_t *d, uint32_t slot)
{
    smp_alloc_desc_t copy;
    uint32_t crc;
    if (d->magic != SMP_ALLOC_MAGIC ||
        d->version != SMP_ALLOC_VERSION ||
        d->logical_slot != slot ||
        !d->sector_count ||
        d->sector_count > SMP_USER_SECTOR_COUNT ||
        d->data_len > (uint32_t)d->sector_count * SMP_USER_SECTOR_SIZE - SMP_USER_DATA ||
        d->commit != SMP_ALLOC_COMMITTED)
        return 0;
    copy = *d;
    copy.record_crc = 0u;
    crc = st_crc32(&copy, 28u);
    return crc == d->record_crc;
}

/* Validate a committed extent against its legacy sample header and audio. */
static int smp_alloc_extent_valid(uint32_t phys, uint32_t slot,
                                  const smp_alloc_desc_t *d)
{
    const uint8_t *base;
    const smp_user_hdr_t *h;
    uint32_t capacity;

    if (phys < SMP_USER_BASE ||
        phys - SMP_USER_BASE >= SMP_USER_POOL_SIZE ||
        (uint32_t)d->sector_count * SMP_USER_SECTOR_SIZE >
            SMP_USER_POOL_SIZE - (phys - SMP_USER_BASE) ||
        !smp_alloc_desc_valid(d, slot))
        return 0;

    base = fm1_xip_ptr(phys);
    h = (const smp_user_hdr_t *)base;
    capacity = (uint32_t)d->sector_count * SMP_USER_SECTOR_SIZE - SMP_USER_DATA;
    if (h->magic != SMP_USER_MAGIC || h->version != 1u ||
        !h->nz || h->nz > 16u || h->data_len != d->data_len ||
        h->data_len > capacity || h->crc != d->data_crc)
        return 0;
    if (st_crc32(base + SMP_USER_DATA, h->data_len) != h->crc)
        return 0;
    return 1;
}

/* Return the first contiguous free run, or -1 if none fits. */
static int smp_alloc_find(uint32_t count)
{
    uint32_t start, run = 0;
    if (!count || count > SMP_USER_SECTOR_COUNT)
        return -1;
    for (start = 0; start < SMP_USER_SECTOR_COUNT; start++) {
        if (!smp_alloc_test(start)) {
            if (++run == count)
                return (int)(start + 1u - count);
        } else {
            run = 0;
        }
    }
    return -1;
}

/* Find and reserve the largest currently free contiguous run. */
static int smp_alloc_find_largest(uint32_t *out_start, uint32_t *out_count)
{
    uint32_t i, run_start = 0, run_count = 0;
    uint32_t best_start = 0, best_count = 0;

    for (i = 0; i <= SMP_USER_SECTOR_COUNT; i++) {
        if (i < SMP_USER_SECTOR_COUNT && !smp_alloc_test(i)) {
            if (!run_count)
                run_start = i;
            run_count++;
        } else {
            if (run_count > best_count) {
                best_start = run_start;
                best_count = run_count;
            }
            run_count = 0;
        }
    }
    if (!best_count)
        return -1;
    *out_start = best_start;
    *out_count = best_count;
    return 0;
}

/* Abandon a staging upload without affecting the active sample. */
static void smp_user_stage_abort(uint32_t slot)
{
    if (slot >= SMP_USER_LOGICAL_SLOTS || smp_user_io_blocked || smp_user_unresolved[slot])
        return;
    /* Journal claims are never recycled by this conservative implementation,
     * even after abort or reboot. Only the live transfer handle is cleared. */
    smp_stage_start[slot] = 0u;
    smp_stage_count[slot] = 0u;
}

/* Reserve a temporary extent. Returns its physical base and byte capacity. */
static int smp_user_stage_begin(uint32_t slot, uint32_t data_len,
                                uint32_t *phys, uint32_t *sectors,
                                uint32_t *capacity)
{
    uint32_t start, count;
    if (slot >= SMP_USER_LOGICAL_SLOTS || smp_journal_state != SMP_JOURNAL_READY ||
        smp_user_io_blocked || smp_user_unresolved[slot] ||
        !phys || !sectors || !capacity ||
        data_len > SMP_USER_POOL_SIZE - SMP_USER_DATA)
        return -1;

    count = (SMP_USER_DATA + data_len + SMP_USER_SECTOR_SIZE - 1u) /
            SMP_USER_SECTOR_SIZE;
    if (!count)
        count = 1u;
    smp_user_stage_abort(slot);
    if (smp_alloc_find(count) < 0)
        return -1;
    start = (uint32_t)smp_alloc_find(count);
    if (!start)
        return -1;                             /* root protection is also a hard range rule */
    if (smp_alloc_reserve(start, count))
        return -1;

    smp_stage_start[slot] = (uint8_t)start;
    smp_stage_count[slot] = (uint8_t)count;
    *phys = SMP_USER_BASE + start * SMP_USER_SECTOR_SIZE;
    *sectors = count;
    *capacity = count * SMP_USER_SECTOR_SIZE;
    return 0;
}

/* Serial ordering is usable only inside an open half-range. */
static int smp_generation_newer(uint32_t a, uint32_t b)
{
    uint32_t delta = a - b;
    return delta != 0u && delta < 0x80000000u;
}

static int smp_generation_can_advance(uint32_t k)
{
    return smp_journal_state == SMP_JOURNAL_READY || !smp_user_has_records[k] ||
        smp_generation_newer(smp_user_generation[k] + 1u, smp_user_oldest_generation[k]);
}

static void smp_generation_committed(uint32_t k, uint32_t generation)
{
    if (!smp_user_has_records[k])
        smp_user_oldest_generation[k] = generation;
    smp_user_has_records[k] = 1u;
    smp_user_generation[k] = generation;
}

#include "sample_journal.c"

/* Only the fixed journal root can authorize sample publication. Unanchored
 * media is preserved in full and never interpreted as sample ownership. */
static void smp_user_alloc_boot(void)
{
    memset(smp_user_occupied, 0, sizeof smp_user_occupied);
    smp_user_io_blocked = 0u;
    for (uint32_t k = 0; k < SMP_USER_LOGICAL_SLOTS; k++) {
        smp_user_phys[k] = smp_user_generation[k] = smp_user_oldest_generation[k] = 0u;
        smp_user_sectors[k] = smp_user_deleted[k] = smp_user_unresolved[k] = 0u;
        smp_user_has_records[k] = 0u;
        smp_stage_start[k] = smp_stage_count[k] = 0u;
    }
    (void)smp_journal_boot();
}

#ifndef SMP_USER_XIP                                /* host tests: a RAM image of the slots */
#define SMP_USER_XIP(k) fm1_xip_ptr(smp_user_phys[k])
#endif
static const uint8_t *smp_user_xip(uint32_t k) { return SMP_USER_XIP(k); }
#if FELUCCA_SLICE
static void slc_user_scan(uint32_t k, int valid);   /* eng_slice.c: SLICE's slice table of the slot */
#else
#define slc_user_scan(k, valid) ((void)0)
#endif
static uint32_t smp_user_gen;                       /* + 1 per slot scan (GRAIN: its seek index follows uploads) */

/* (re)read slot k from flash: valid header -> zones usable; call after boot and after an upload
 * (main loop: SLICE scans the slot's audio here). The zones are checked in a copy: an unusable slot leaves
 * every zone of it empty (n 0: a voice still on it ends), never a half-checked one */
static void smp_user_scan(uint32_t k)
{
    const smp_user_hdr_t *h;
    smp_zone_t zt[16];
    uint32_t i, base, nz = 0, capacity;
    if (k >= SMP_USER_LOGICAL_SLOTS)
        return;
    smp_user_gen++;
    usr_nz[k] = 0;
    slc_user_scan(k, 0);
    if (smp_journal_state != SMP_JOURNAL_READY || smp_user_deleted[k] || !smp_user_phys[k] ||
        !smp_user_sectors[k])
        goto empty;
    h = (const smp_user_hdr_t *)smp_user_xip(k);
    capacity = (uint32_t)smp_user_sectors[k] * SMP_USER_SECTOR_SIZE;
    if (capacity < SMP_USER_DATA)
        goto empty;
    capacity -= SMP_USER_DATA;
    if (h->magic == SMP_USER_MAGIC && h->version == 1u &&
        h->nz && h->nz <= 16u && h->data_len <= capacity) {
        base = (uint32_t)(uintptr_t)(smp_user_xip(k) + SMP_USER_DATA) -
               (uint32_t)(uintptr_t)SMP_DATA;
        for (nz = h->nz, i = 0; i < nz; i++) {
            smp_zone_t *z = &zt[i];
            uint32_t bytes;
            *z = h->zone[i];
            if (z->off > h->data_len) {
                nz = 0;
                break;
            }
            bytes = z->n / 2u + (z->n & 1u);
            if (bytes > h->data_len - z->off ||
                z->idx > 88u || !z->rate || z->rate > 4u << 16 ||
                z->ls > z->le || z->le >= z->n || z->lo > z->hi) {
                nz = 0;
                break;
            }
            z->off += base;
        }
    }
    for (i = 0; i < 16u; i++)
        usr_zone[k][i] = i < nz ? zt[i] : (smp_zone_t){0};
    if (!nz)
        return;
    usr_nz[k] = (uint8_t)nz;
    slc_user_scan(k, 1);
    return;

empty:
    for (i = 0; i < 16u; i++)
        usr_zone[k][i] = (smp_zone_t){0};
}

/* A missing sample (SAMPLE, GRAIN, SLICE: an empty or invalid user slot, a set this build has no data for) plays
 * this instead, like a hardware sampler with its sample gone: a plain sine at the note's pitch (+ d16, 1/16
 * semitones), at half the peak of a full sample, through the engine's one-pole low-pass (lp Q15, state *y) and the
 * voice's amp (the ADSR: no click at the start, phase 0, or the end). *ph its phase. The UI says so once
 * (engines.c snd_missing, ui_input.c sample_notice). */
static void smp_sine(int32_t *out, uint32_t n, const vmod_t *m, int32_t d16, int32_t lp, uint32_t *ph, int32_t *y)
{
    uint32_t i, p = *ph, inc = pitch_inc((uint32_t)clamp(m->pitch16 + d16, 0, 2047));
    int32_t s = *y;
    for (i = 0; i < n; i++, p += inc) {
        s += mulq15((sine_i(p) >> 1) - s, lp);
        out[i] += voice_amp(s, m, i) << 1;
    }
    *ph = p;
    *y = s;
}
/* SET / SRC si (0..SMP_NALL - 1) has no sample data: an empty or invalid user slot, a built-in set without data */
static int smp_set_missing(uint32_t si)
{
    if (si < SMP_NSETS)
        return !SMP_ZONES[SMP_SETS[si].z0].n;
    return !usr_nz[(si - SMP_NSETS) % SMP_USER_SLOTS];
}

/* zone index in a voice: < 0x8000 built-in, else 0x8000 | slot << 5 | zone */
static inline const smp_zone_t *smp_zone(uint32_t zi)
{
    return zi < 0x8000u ? &SMP_ZONES[zi] : &usr_zone[(zi >> 5) & 7u][zi & 31u];
}

/* voice: ph[0] position (samples), ph[1] fraction Q16, s[0] predictor, s[1] step
 * index, s[2] previous sample, s[3] current sample, s[4] zone, s[5] step Q16 */
static inline int32_t sample_next(const smp_zone_t *z, voice_t *v, int loop)
{
    uint32_t pos = v->ph[0], b = SMP_DATA[z->off + (pos >> 1)];
    uint32_t code = (pos & 1u) ? (b >> 4) : (b & 15u);
    int32_t step = IMA_STEP[(uint32_t)v->s[1] <= 88u ? v->s[1] : 88], vd = step >> 3;
    if (code & 4u)
        vd += step;
    if (code & 2u)
        vd += step >> 1;
    if (code & 1u)
        vd += step >> 2;
    v->s[0] = clamp(v->s[0] + ((code & 8u) ? -vd : vd), -32768, 32767);
    v->s[1] = clamp(v->s[1] + IMA_IDX[code & 7u], 0, 88);
    pos++;
    if (pos > z->le && z->looped && loop) {
        pos = z->ls;
        v->s[0] = z->pred;
        v->s[1] = z->idx;
    }
    v->ph[0] = pos;
    return v->s[0];
}

static void sample_note_on(track_t *t, voice_t *v)
{
    uint32_t si = (uint32_t)t->p[P_E0] % SMP_NALL, i, zi = 0xFFFFu, width = 128u;
    if (si < SMP_NSETS) {                           /* a built-in set: its zones split the keyboard */
        const smp_set_t *set = &SMP_SETS[si];
        for (i = 0; i < set->nz; i++)
            if (v->note >= SMP_ZONES[set->z0 + i].lo && v->note <= SMP_ZONES[set->z0 + i].hi) {
                zi = set->z0 + i;
                break;
            }
        v->s[4] = (int32_t)(zi == 0xFFFFu ? set->z0 : zi);
    } else {                                        /* user slot: silent if empty; the narrowest zone plays */
        uint32_t k = si - SMP_NSETS;
        for (i = 0; i < usr_nz[k]; i++) {
            const smp_zone_t *z = &usr_zone[k][i];
            if (v->note >= z->lo && v->note <= z->hi && z->hi - z->lo <= width) {
                zi = 0x8000u | k << 5 | i;
                width = z->hi - z->lo;
            }
        }
        v->s[4] = (int32_t)(zi == 0xFFFFu ? 0 : zi);
    }
    v->ph[0] = 0;
    v->ph[1] = 0;
    v->s[0] = 0;
    v->s[1] = 0;
    v->s[2] = v->s[3] = 0;
    v->s[6] = zi == 0xFFFFu;     /* 1 = sample ended (one-shot); a kit key with no sound stays silent */
    v->s[7] = 0;                 /* lo-pass state */
    if (smp_set_missing(si))
        v->s[6] = 2;             /* 2 = no sample data: the sine (smp_sine; ph[0] its phase) */
}

static void sample_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    const smp_zone_t *z = smp_zone((uint32_t)v->s[4]);
    uint32_t i, frac = v->ph[1];
    int32_t d16 = clamp(m->pitch16 + p[P_E1] * 16 - z->root16, -1536, 576);   /* <= 3 octaves up: bounded decode load */
    uint32_t r = pow2_q16(d16), stepq = (r >> 8) * (z->rate >> 8);   /* Q16 samples per output */
    int32_t bits = p[P_E2], lp = 4000 + ((clamp((p[P_E4] << 8) + m->cutoff, 0, 127 << 8) * 28767) >> 15);
    int32_t drv = p[P_E6], sh = bits / 10;
    if (v->s[6] == 2) {                               /* no sample data: the sine at the note's pitch + TUNE */
        smp_sine(out, n, m, p[P_E1] * 16, lp, &v->ph[0], &v->s[7]);
        return;
    }
    if (v->s[6] || !z->n) {
        v->active = 0;
        return;
    }
    if (v->ph[0] == 0 && v->ph[1] == 0 && v->s[3] == 0)
        v->s[3] = sample_next(z, v, p[P_E3]);         /* prime the interpolator */
    for (i = 0; i < n; i++) {
        int32_t s;
        frac += stepq;
        while (frac >= 65536u) {
            frac -= 65536u;
            v->s[2] = v->s[3];
            if (v->ph[0] >= z->n) {                   /* one-shot reached its end */
                v->s[6] = 1;
                v->s[3] = 0;
                if (frac >= 65536u) {                /* past the last sample-to-zero interval */
                    v->s[2] = 0;
                    frac &= 65535u;
                }
                break;
            }
            v->s[3] = sample_next(z, v, p[P_E3]);
        }
        s = v->s[2] + (((v->s[3] - v->s[2]) * (int32_t)(frac >> 1)) >> 15);
        if (sh)                                       /* BITS: 0 = clean, up to 12 bits removed */
            s = (s >> sh) << sh;
        if (drv)
            s = softclip(s + (((s >> 2) * (drv * 150)) >> 11));   /* = s * drv * 600 / 32768, no overflow */
        v->s[7] += mulq15(s - v->s[7], lp);           /* gentle tone control (CUT) */
        out[i] += voice_amp(v->s[7], m, i) << 1;
        if (v->s[6])
            break;
    }
    v->ph[1] = frac;
}

static const engine_t ENG_SAMPLE = {
    .name = "SAMPLE",
    .page_title = {"SET", "TONE"},
    .edit = {
        {"SET", F_ENUM, 0, SMP_NALL - 1, 0, SMP_ALL_NAMES, 0},
        {"TUNE", F_SEMI, -24, 24, 0, 0, 0},
        {"BITS", F_INT, 0, 127, 0, 0, 0},
        {"LOOP", F_ENUM, 0, 1, 1, N_ONOFF_E, 0},
        {"CUT", F_INT, 0, 127, 127, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
        {"DRV", F_PCT, 0, 127, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
    },
    .presets = SMP_PRESET_TABLE,
    .npresets = SMP_NPRESETS,
    .note_on = sample_note_on,
    .render = sample_render,
    .knob = {P_E0, P_E4, P_ATK, P_REL},
    .sampled = 1,
};
