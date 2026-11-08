/* SPDX-License-Identifier: GPL-3.0-only */
/* Test-only five-source header supplied by factory_pipeline_test.py. */
#define SAMPLE_XIP_TEST_NO_MAIN 1
#include "../web/emu/sample_xip_test.c"
#include "old_pack.h"
#include "factory_fixture_expected.h"

static int bank_layout(uint32_t unused)
{
    (void)unused;
    if (SMP_NSETS != 5 || SMP_NALL != 13 || SMP_USER_SLOTS != 8 || SMP_USER_POOL_SIZE != 245760) return 0;
    for (uint32_t i = 0; i < 5; i++) {
        if (strcmp(SMP_SETS[i].name, fixture_names[i]) || SMP_SET_ORIG[i] != i || SMP_SETS[i].nz != 1) return 0;
        const smp_zone_t *z = &SMP_ZONES[SMP_SETS[i].z0];
        if (SMP_SETS[i].z0 >= NELEM(SMP_ZONES) || z->off > sizeof SMP_DATA ||
            (z->n + 1u) / 2u > sizeof SMP_DATA - z->off || !!z->n != fixture_present[i]) return 0;
    }
    const param_desc_t *d[] = {&ENG_SAMPLE.edit[0], &ENG_GRAIN.edit[0]};
    for (uint32_t e = 0; e < 2; e++) {
        if (d[e]->min != 0 || d[e]->max != 12) return 0;
        for (int32_t i = 0; i <= 12; i++) if (param_fit(d[e], i) != i) return 0;
        if (param_fit(d[e], 13) != 12 || param_fit(d[e], -1) != 0 || enum_orig(d[e], 4) != 4 || enum_orig(d[e], 5) != 5) return 0;
    }
    return 1;
}
static int decode_source(uint32_t src)
{
    fixture(0); web_boot();
    const smp_zone_t *z = &SMP_ZONES[SMP_SETS[src].z0];
    track_t *t = &trk[0]; host_preset(t, ENGI_SAMPLE, 0); t->p[P_E0] = src;
    voice_t v = {0}; v.note = 60; sample_note_on(t, &v);
    if (!z->n) return v.s[6] == 2 && gr_find(src, 60) == GR_SINE && unchanged();
    if (v.s[6] || smp_zone(v.s[4]) != z || gr_find(src, 60) != 0) return 0;
    uint32_t crc = 0xFFFFFFFFu; int32_t pred = 0, idx = 0;
    for (uint32_t i = 0; i < z->n; i++) {
        int16_t value = (int16_t)sample_next(z, &v, 0);
        if (gr_dec(z, i, &pred, &idx) != value || (i < 32 && value != fixture_prefix[src][i])) return 0;
        uint8_t le[2] = {(uint8_t)value, (uint8_t)((uint16_t)value >> 8)};
        crc = st_crc32_add(crc, le, 2);
    }
    if (~crc != fixture_crc[src]) return 0;
    if (z->looped) {
        memset(&v, 0, sizeof v);
        for (uint32_t i = 0; i <= z->le; i++) sample_next(z, &v, 1);
        if (v.ph[0] != z->ls || v.s[0] != z->pred || v.s[1] != z->idx) return 0;
        for (uint32_t i = 0; i < 16; i++) if (sample_next(z, &v, 0) != fixture_loop[src][i]) return 0;
    }
    return unchanged();
}
static int render_factory(uint32_t which)
{
    uint32_t src = which % 5, engine = which < 5 ? ENGI_SAMPLE : ENGI_GRAIN;
    fixture(0); web_boot();
    for (uint32_t i = 0; i < 180; i++) web_render(128);
    track_t *t = &trk[0]; host_preset(t, engine, 0); t->p[P_E0] = src; web_keys(1u << 7);
    float peak = 0;
    for (uint32_t b = 0; b < 40; b++) {
        web_render(128);
        for (uint32_t i = 0; i < 128; i++) {
            float x = fabsf(out_l[i]) + fabsf(out_r[i]);
            if (!isfinite(x)) return 0;
            if (x > peak) peak = x;
        }
    }
    web_keys(0);
    return peak > 0.00001f && unchanged();
}
static int compatibility_limits(uint32_t unused)
{
    (void)unused; fixture(0); web_boot();
    static project_t q, decoded; static project_store_t packed;
    for (uint32_t e = 0; e < 2; e++) for (uint32_t src = 0; src < 13; src++) {
        host_preset(&trk[0], e ? ENGI_GRAIN : ENGI_SAMPLE, 0); trk[0].p[P_E0] = src; project_capture(&q);
        if (!proj_pack(&packed, &q) || !proj_import(&decoded, &packed, sizeof packed) || decoded.t[0].p[P_E0] != src) return 0;
    }
    host_preset(&trk[0], ENGI_SAMPLE, 0); trk[0].p[P_E0] = 4; project_capture(&q);
    if (!old_pack10(packed.raw, &q, 111) || !proj_import(&decoded, &packed, sizeof packed) || decoded.t[0].engine != ENGI_DRUM) return 0;
    /* Detect existing restrictions, rather than pretending a generated bank
     * alone enables the fifth factory preset or changes its legacy meaning. */
    if (ENG_SAMPLE.npresets != 4) return 0;
    apply_preset_to(&trk[0], 4);
    return trk[0].eng_req == ENGI_DRUM && unchanged();
}
static int byte_bounds(uint32_t unused)
{
    (void)unused; fixture(0); web_boot();
    if (smp_data_byte(0) != SMP_DATA[0] || smp_data_byte(sizeof SMP_DATA - 1u) != SMP_DATA[sizeof SMP_DATA - 1u]) return 0;
    const uint8_t *pool = fm1_xip_ptr(SMP_USER_BASE);
    uint32_t rebased = (uint32_t)(uintptr_t)pool - (uint32_t)(uintptr_t)SMP_DATA;
    if (smp_data_byte(rebased + SMP_USER_POOL_SIZE) != 0) return 0;
    for (uint32_t k = 0; k < 8; k++) {
        uint32_t off = usr_zone[k][0].off;
        if (smp_data_byte(off) != smp_user_xip(k)[512] || smp_data_byte(off + 2047u) != smp_user_xip(k)[512 + 2047u]) return 0;
    }
    return unchanged();
}
#ifdef FACTORY_TABLE_MUTABLE_TEST
static int invalid_factory(uint32_t which)
{
    fixture(0); web_boot();
    smp_set_t saved = SMP_SETS[0]; smp_zone_t zone = SMP_ZONES[0], later = SMP_ZONES[1];
    switch (which) {
    case 0: SMP_SETS[0].z0 = 65535; break;
    case 1: SMP_SETS[0].nz = 65535; break;
    case 2: SMP_SETS[0].nz = 0; break;
    case 3: SMP_ZONES[0].off = UINT32_MAX; break;
    case 4: SMP_ZONES[0].n = UINT32_MAX; break;
    case 5: SMP_ZONES[0].idx = 89; break;
    case 6: SMP_ZONES[0].rate = 0; break;
    case 7: SMP_ZONES[0].root16 = -1; break;
    case 8: SMP_ZONES[0].hi = 128; break;
    case 9: SMP_ZONES[0].looped = 1; SMP_ZONES[0].ls = SMP_ZONES[0].le; break;
    case 10: SMP_ZONES[0].off = usr_zone[7][0].off; break;
    case 11: SMP_SETS[0].nz = 2; SMP_ZONES[1].off = UINT32_MAX; break;
    }
    track_t *t = &trk[0]; host_preset(t, ENGI_SAMPLE, 0); t->p[P_E0] = 0;
    voice_t v = {0}; v.note = 60; sample_note_on(t, &v);
    int ok = smp_set_missing(0) && v.s[6] == 2 && gr_nz(0) == 0 && gr_find(0, 60) == GR_SINE && sample_wave_zone(t) == 0xFFFFu;
    SMP_SETS[0] = saved; SMP_ZONES[0] = zone; SMP_ZONES[1] = later;
    return ok && unchanged();
}
#endif
int main(int argc, char **argv)
{
    int decode_only = argc > 1 && !strcmp(argv[1], "--decode-only");
#ifdef FACTORY_TABLE_MUTABLE_TEST
    for (uint32_t k = 0; k < 12; k++) run("invalid factory descriptors use fallback before access", invalid_factory, k, 0);
#endif
    run("bounded factory and actual NOR object reads", byte_bounds, 0, 0);
    run("private five-source layout, fixed IDs and 4/5/12/13 boundaries", bank_layout, 0, 0);
    for (uint32_t src = 0; src < 5; src++) {
        char name[96]; snprintf(name, sizeof name, "source %u: complete SAMPLE/GRAIN decode and loop state", src);
        run(name, decode_source, src, 0);
    }
    for (uint32_t i = 0; !decode_only && i < 10; i++) {
        char name[96]; snprintf(name, sizeof name, "%s source %u: finite nonzero audio and preserved NOR", i < 5 ? "SAMPLE" : "GRAIN", i % 5);
        run(name, render_factory, i, 0);
    }
    for (uint32_t i = 0; !decode_only && i < (FELUCCA_SLICE ? 24u : 16u); i++) {
        char name[96]; snprintf(name, sizeof name, "all-slot audio: engine %u USR%u", i / 8, i % 8 + 1);
        run(name, render_slot, i, 0);
    }
    run("all eight relocated v2 user sources still recover and decode", valid_boot, 0, 0);
    run("FUNB source IDs round-trip; legacy/preset-4 restrictions remain explicit", compatibility_limits, 0, 0);
    return failures != 0;
}
