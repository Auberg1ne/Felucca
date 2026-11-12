/* SPDX-License-Identifier: GPL-3.0-only */
/* Actual simulator/firmware TU; forked cases mirror fresh WASM instances. */
#define SAMPLE_XIP_TEST_NO_MAIN 1
#include "../web/emu/sample_xip_test.c"
#include "old_pack.h"

static int source_ids(uint32_t unused)
{
    (void)unused;
    static const uint32_t ids[8] = {1, 2, 3, 5, 6, 7, 8, 9};
    fixture(0); web_boot();
    if (SMP_NSETS != 5 || SMP_NALL != 13 || SLC_SRC_PIANO != 4 || SLC_NSRC != 10 || SLC_SINE != 15) return 0;
    if (ENG_SAMPLE.edit[0].max != 12 || ENG_GRAIN.edit[0].max != 12 || ENG_SLICE.edit[0].max != 9) return 0;
    if (slc_slot_from_src(0) != 8 || slc_slot_from_src(4) != 8 || slc_slot_from_src(10) != 8 ||
        slc_slot_from_src(UINT32_MAX) != 8 || slc_src_from_slot(8) != 10) return 0;
    for (uint32_t k = 0; k < 8; k++) {
        char name[] = "USR1"; name[3] += k;
        if (slc_src_from_slot(k) != ids[k] || slc_slot_from_src(ids[k]) != k ||
            strcmp(SMP_ALL_NAMES[SMP_NSETS + k], name) || strcmp(N_SLC_SRC[ids[k]], name) ||
            slc_get(ids[k]) != &slc_usr[k] || gr_zone(SMP_NSETS + k, 0) != &usr_zone[k][0]) return 0;
        const smp_zone_t *z = smp_zone(0x8000u | k << 5 | 15u);
        if (z != &usr_zone[k][15]) return 0; /* unchanged SAMPLE three-bit slot encoding */
    }
    return slc_get(4) == (SLC_PIANO.len ? &SLC_PIANO : 0) && !slc_get(10) && unchanged();
}
static int packed_fields(uint32_t unused)
{
    (void)unused;
    for (uint32_t src = 0; src <= 15; src++)
        for (uint32_t seg = 0; seg < 16; seg++)
            for (uint32_t rev = 0; rev < 2; rev++)
                for (uint32_t j = 0; j < 64; j++)
                    for (uint32_t div = 0; div <= SLC_DIV_MAN; div++) {
                        uint32_t pk = slc_vpack(src, seg, rev, j, div);
                        if (slc_vsrc(pk) != src || slc_vseg(pk) != seg || slc_vrev(pk) != rev ||
                            slc_vslice(pk) != j || slc_vdiv(pk) != div) return 0;
                        uint32_t next = (pk & ~(15u << SLC_VSEG_SHIFT)) | ((seg + 1u) % 16u) << SLC_VSEG_SHIFT;
                        if (slc_vsrc(next) != src || slc_vrev(next) != rev || slc_vslice(next) != j || slc_vdiv(next) != div) return 0;
                    }
    return 1;
}
static int edits_and_missing(uint32_t k)
{
    fixture(0); web_boot(); host_preset(&trk[0], 13, 0); song.sel = 0;
    trk[0].p[P_E0] = slc_src_from_slot(k); trk[0].p[P_E1] = SLC_DIV_MAN;
    uint32_t selected = 99, writes = web_flash_writes;
    slc_man_t *m = slice_edit_begin(&selected);
    const slc_src_t *s = slc_get(slc_src_from_slot(k));
    if (!m || selected != k || !s || !m->n) return 0;
    slc_man_split(s, m, 0); slc_man_move(s, m, 1, 7); slice_edit_end(k);
    if (!slc_man_of(s) || !(slc_man_save & (1u << k))) return 0;
    for (uint32_t other = 0; other < 8; other++)
        if (other != k && slc_man_of(&slc_usr[other])) return 0;
    ui.home = 1; slc_store_poll();
    if (slc_man_save || strcmp(ui.msg, "SLICES RAM ONLY") || web_flash_writes != writes || !unchanged()) return 0;
    usr_nz[k] = 0; slc_usr[k].len = 0;
    if (!smp_set_missing(SMP_NSETS + k) || gr_nz(SMP_NSETS + k)) return 0;
    if (slice_edit_begin(&selected)) return 0;
    char name[] = "USR1 EMPTY"; name[3] += k;
    if (strcmp(ui.msg, name)) return 0;
    voice_t *v = &trk[0].v[0]; memset(v, 0, sizeof *v); v->note = 60; v->stage = 3;
    slice_note_on(&trk[0], v);
    if (slc_vsrc(v->s[4]) != SLC_SINE) return 0;
    slice_amp(&trk[0], v, 32767);
    if (v->stage != 3) return 0; /* ONE must not consume the missing-source sine */
    vmod_t mod = {0}; mod.pitch16 = 60 * 16; mod.amp0 = mod.amp1 = 32767;
    int32_t out[32] = {0}; v->active = 1;
    slice_render(&trk[0], v, out, 32, &mod);
    int sounding = 0;
    for (uint32_t i = 0; i < 32; i++) sounding |= out[i] != 0;
    if (!sounding || slc_vsrc(v->s[4]) != 15) return 0;
    voice_t missing = {0}; missing.note = 60; trk[0].p[P_E0] = SMP_NSETS + k;
    sample_note_on(&trk[0], &missing);
    if (missing.s[6] != 2 || gr_find(SMP_NSETS + k, 60) != GR_SINE) return 0;
    trk[0].p[P_E0] = slc_src_from_slot(k);
    smp_user_scan(k); slice_note_on(&trk[0], v); v->stage = 3;
    slice_amp(&trk[0], v, 32767);
    return slc_vsrc(v->s[4]) == slc_src_from_slot(k) && v->stage == 2 && unchanged();
}
static int segment_and_reverse(uint32_t k)
{
    fixture_multi_slot = k; fixture(0); web_boot(); host_preset(&trk[0], 13, 0);
    track_t *t = &trk[0]; t->p[P_E0] = slc_src_from_slot(k); t->p[P_E1] = SLC_DIV_MAN;
    t->p[P_E4] = SLC_ONE; t->p[P_E6] = 127;
    const slc_src_t *s = slc_get(slc_src_from_slot(k));
    if (!s || s->nseg != 16) return 0;
    slc_man_t *man = slc_man_begin(k); if (!man) return 0;
    man->n = 1; man->pos[0] = man->st[0] = man->end = 0; slc_man_commit(k);
    for (uint32_t rev = 0; rev < 2; rev++) {
        t->p[P_E5] = rev; voice_t *v = &t->v[0]; memset(v, 0, sizeof *v);
        v->active = v->gate = 1; v->note = 60; v->pitch_cur = 60 * 16;
        slice_note_on(t, v); uint32_t maxseg = 0; int nonzero = 0;
        vmod_t mod = {0}; mod.pitch16 = 60 * 16; mod.amp0 = mod.amp1 = 32767;
        for (uint32_t block = 0; block < 300 && v->active; block++) {
            int32_t out[32] = {0}; slice_render(t, v, out, 32, &mod);
            uint32_t pk = v->s[4];
            if (slc_vsrc(pk) != slc_src_from_slot(k) || slc_vrev(pk) != rev || slc_vdiv(pk) != SLC_DIV_MAN || slc_vslice(pk)) return 0;
            if (slc_vseg(pk) > maxseg) maxseg = slc_vseg(pk);
            for (uint32_t i = 0; i < 32; i++) nonzero |= out[i] != 0;
        }
        if (!nonzero || (!rev && maxseg != 15)) return 0;
    }
    return unchanged();
}
static void make_project(project_t *q, uint32_t engine, uint32_t src)
{
    memset(q, 0, sizeof *q); q->magic = PROJ_MAGIC; q->size = sizeof *q;
    q->parts = NPART; q->phys = PROJ_PHYS; chain_defaults(&q->chain);
    for (uint32_t i = 0; i < G_COUNT; i++) q->g[i] = GP[i].def;
    for (uint32_t t = 0; t < NTRK; t++) {
        uint32_t e = t ? ENGI_GRAIN : engine; q->t[t].engine = e;
        for (uint32_t p = 0; p < P_COUNT; p++) q->t[t].p[p] = param_desc_of(e, p)->def;
        memcpy(q->fm6[t], FM6_INIT, FM6_PACKED);
    }
    q->t[0].p[P_E0] = src;
    q->motion.count = 1; q->motion.on = 1;
    q->motion.event[0].param = P_E0 | MOTION_LOCK; q->motion.event[0].value = src;
    q->sum = proj_sum(q);
}
static int stored_sources(uint32_t which)
{
    web_nor_erase(); web_boot(); uint32_t engine = which == 0 ? ENGI_SAMPLE : which == 1 ? ENGI_GRAIN : 13;
    uint32_t max = engine == 13 ? 9 : 12;
    static project_t q, decoded, again; static uint8_t a[3840], b[3840];
    for (uint32_t src = 0; src <= max; src++) {
        make_project(&q, engine, src);
        uint32_t len = old_pack(a, &q, OLD_FUN9, 99);
        if (!len || !proj_import(&decoded, a, len)) return 0;
        if (engine == ENGI_SAMPLE && src == SMP_SET_PERC) {
            if (decoded.t[0].engine != ENGI_DRUM) return 0; /* preserve existing retired-kit migration */
            continue;
        }
        if (decoded.t[0].engine != engine || decoded.t[0].p[P_E0] != src ||
            MOTION_ID(&decoded.motion.event[0]) != P_E0 || decoded.motion.event[0].value != src ||
            !(decoded.motion.event[0].param & MOTION_LOCK)) return 0;
        if (old_pack(b, &decoded, OLD_FUN9, 99) != len || memcmp(a, b, len) ||
            !proj_import(&again, b, len) || again.t[0].p[P_E0] != src) return 0;
        if (project_restore_runtime(&decoded) || trk[0].p[P_E0] != src) return 0;
        if (motion_set_lock(&trk[0], 3, P_E0, src)) return 0;
        int16_t value = -1;
        if (!motion_lock_get(&trk[0], 3, P_E0, &value) || value != src) return 0;
        up_rec_t preset = {0}; preset.used = UP_USED; preset.ver = UP_VER; preset.engine = engine;
        preset.np = P_COUNT; preset.name[0] = 'S';
        for (uint32_t p = 0; p < P_COUNT; p++) up_set_value(&preset, p, q.t[0].p[p]);
        int16_t values[P_COUNT];
        if (!up_valid(&preset)) return 0;
        up_values(&preset, values);
        if (values[P_E0] != src || up_value(&preset, P_E0) != src) return 0;
    }
    return PROJ_MAGIC == 0x46554E41u && PROJ_STORE_SIZE == 3840 && SMP_USER_POOL_SIZE == 0x3C000u;
}
int main(void)
{
    struct rlimit limit = {0, 0}; setrlimit(RLIMIT_CORE, &limit);
    run("S2 stable source IDs, names, PIANO and SAMPLE slot encoding", source_ids, 0, 0);
    run("S2 exhaustive transient voice field isolation", packed_fields, 0, 0);
    for (uint32_t k = 0; k < 8; k++) {
        char name[100]; snprintf(name, sizeof name, "S2 USR%u manual edits, RAM-only notification and missing-source sine", k + 1);
        run(name, edits_and_missing, k, 0);
        snprintf(name, sizeof name, "S2 USR%u forward/reverse playback across all sixteen segments", k + 1);
        run(name, segment_and_reverse, k, 0);
    }
    for (uint32_t e = 0; e < 3; e++) run("S2 legacy FUN9 source round trips, runtime load, presets and locks", stored_sources, e, 0);
    printf("S2 source IDs unchanged; FUN11 current-format saves are covered by project_format_test.\n");
    return failures ? 1 : 0;
}
