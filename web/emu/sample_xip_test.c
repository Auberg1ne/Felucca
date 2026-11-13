/* SPDX-License-Identifier: GPL-3.0-only */
/* The actual simulator TU, as native_check.c builds it. Each fork starts from
 * pristine module globals, like a fresh WASM instance (web_boot is one-shot).
 * cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src
 *    web/emu/sample_xip_test.c -lm -o /tmp/sample_xip_test */
#include "felucca_web.c"
#include <signal.h>
#include <sys/resource.h>

static const uint32_t extent[8] = {3, 11, 27, 35, 41, 49, 54, 58};
static uint8_t pool_before[SMP_USER_POOL_SIZE];
static int failures;
static uint32_t fixture_multi_slot = 8;

/* Explicit v2 boot fixtures, not a second production upload implementation.
 * Real storage/CRC types and NOR programming semantics are used. No editor
 * transport is added to the simulator; upload/power-cut coverage stays in
 * tests/sample_delete_test.c. Offsets intentionally differ from fixed slots. */
static void record(uint32_t off, smp_alloc_desc_t *d)
{
    d->commit = SMP_ALLOC_COMMITTED;
    d->record_crc = 0;
    d->record_crc = st_crc32(d, 28);
    if (st_prog(off, d, sizeof *d)) abort();
}
static void fixture(int old_publication)
{
    web_nor_erase();
    smp_alloc_desc_t root = smp_journal_header();
    record(SMP_USER_BASE, &root);
    for (uint32_t k = 0; k < 8; k++) {
        uint32_t phys = SMP_USER_BASE + extent[k] * SMP_USER_SECTOR_SIZE;
        uint8_t audio[2048]; memset(audio, k == 7 ? 0x21u : (k + 1u) * 0x11u, sizeof audio);
        smp_user_hdr_t h = {0};
        h.magic = SMP_USER_MAGIC; h.version = 1; h.nz = 1;
        h.name[0] = 'A' + k; h.data_len = sizeof audio; h.crc = st_crc32(audio, sizeof audio);
        h.zone[0].n = sizeof audio * 2u; h.zone[0].le = h.zone[0].n - 1u;
        h.zone[0].rate = 32768; h.zone[0].root16 = 60 * 16; h.zone[0].hi = 127;
        if (k == fixture_multi_slot) {
            h.nz = 16; h.zone[0].n = 256; h.zone[0].le = 255;
            for (uint32_t z = 1; z < 16; z++) { h.zone[z] = h.zone[0]; h.zone[z].off = z * 128; }
        }
        smp_alloc_desc_t claim = {0};
        claim.magic = SMP_JOURNAL_CLAIM; claim.version = SMP_JOURNAL_V1;
        claim.logical_slot = k; claim.sector_count = 1; claim.generation = 1;
        claim.data_len = h.data_len; claim.reserved = extent[k];
        record(SMP_USER_BASE + 32 + k * 64, &claim);
        if (st_prog(phys + SMP_USER_DATA, audio, sizeof audio) || st_prog(phys, &h, sizeof h)) abort();
        smp_alloc_desc_t d = claim;
        d.magic = SMP_ALLOC_MAGIC; d.version = SMP_ALLOC_VERSION;
        d.data_crc = h.crc; d.reserved = st_crc32(&h, sizeof h);
        if (old_publication && !k) { d.version = 1; d.reserved = 0; }
        record(phys + SMP_ALLOC_OFFSET, &d);
        smp_alloc_desc_t pub = d;
        pub.magic = SMP_JOURNAL_PUBLISH; pub.version = SMP_JOURNAL_PUBLISH_VERSION;
        pub.data_crc = d.reserved; pub.reserved = extent[k];
        if (old_publication && !k) { pub.version = 1; pub.data_crc = h.crc; }
        record(SMP_USER_BASE + 64 + k * 64, &pub);
    }
    memcpy(pool_before, nor + SMP_USER_BASE, sizeof pool_before);
}
static int unchanged(void)
{
    return !memcmp(pool_before, nor + SMP_USER_BASE, sizeof pool_before);
}
static int mapping(uint32_t unused)
{
    (void)unused; web_nor_erase();
    const uint32_t off[] = {0, 0x4000, SMP_USER_BASE, SMP_USER_BASE + SMP_USER_POOL_SIZE - 1, sizeof nor - 1};
    for (uint32_t i = 0; i < NELEM(off); i++)
        if (fm1_xip_ptr(off[i]) != nor + off[i] || *fm1_xip_ptr(off[i]) != 255) return 0;
    return 1;
}
static int erased_boot(uint32_t unused)
{
    (void)unused; web_nor_erase(); web_boot();
    if (!flash_ok || smp_journal_state != SMP_JOURNAL_BLANK || !web_booted || web_flash_writes) return 0;
    for (uint32_t k = 0; k < 8; k++) if (usr_nz[k] || smp_user_phys[k] || smp_user_header_bound[k]) return 0;
    for (uint32_t i = 0; i < sizeof nor; i++) if (nor[i] != 255) return 0;
    return SMP_USER_SLOTS == 8 && SMP_USER_POOL_SIZE == 0x3C000u && SMP_USER_SECTOR_COUNT == 60 && SLC_SRC_PIANO == 4 && SLC_NSRC == 10;
}
static int valid_boot(uint32_t unused)
{
    (void)unused; fixture(0); web_boot();
    if (!flash_ok || smp_journal_state != SMP_JOURNAL_READY || !unchanged()) return 0;
    static const int32_t first[8] = {1, 3, 4, 7, 8, 10, 11, 1}; /* IMA codes 1, 2, 3 from initial state 0/0 */
    for (uint32_t k = 0; k < 8; k++) {
        uint32_t phys = SMP_USER_BASE + extent[k] * SMP_USER_SECTOR_SIZE;
        if (smp_user_phys[k] != phys || smp_user_xip(k) != nor + phys || usr_nz[k] != 1 ||
            !smp_user_header_bound[k] || smp_user_unresolved[k] || !smp_alloc_test(extent[k])) return 0;
        track_t *t = &trk[0]; voice_t v = {0}; v.note = 60;
        t->p[P_E0] = SMP_NSETS + k; sample_note_on(t, &v);
        if (v.s[6] || smp_zone(v.s[4]) != &usr_zone[k][0] || sample_next(smp_zone(v.s[4]), &v, 0) != first[k]) return 0;
        const smp_zone_t *z = gr_zone(SMP_NSETS + k, 0); int32_t pred = 0, idx = 0;
        if (gr_find(SMP_NSETS + k, 60) != 0 || z != &usr_zone[k][0] || gr_dec(z, 0, &pred, &idx) != first[k]) return 0;
        const slc_src_t *s = slc_get(k < 3 ? k + 1 : k + 2); slc_dec_t dec;
        slc_dec_at(&dec, 0, 0);
        if (!s || s->len != 4096 || slc_dec_next(s, &dec) != first[k]) return 0;
    }
    return unchanged();
}
static int render_slot(uint32_t which)
{
    fixture(0); web_boot();
    for (uint32_t n = 0; n < 1400; n++) web_render(128); /* past splash/keyboard hold */
    uint32_t k = which % 8, engine = which / 8 == 0 ? ENGI_SAMPLE : which / 8 == 1 ? ENGI_GRAIN : 13;
    track_t *t = &trk[0]; host_preset(t, engine, 0);
    t->p[P_E0] = engine == 13 ? (k < 3 ? k + 1 : k + 2) : SMP_NSETS + k;
    song.sel = 0; web_keys(1u << 7); /* C4, as the existing simulator test */
    float peak = 0;
    for (uint32_t n = 0; n < 40; n++) {
        web_render(128);
        for (uint32_t i = 0; i < 128; i++) {
            float x = fabsf(out_l[i]) + fabsf(out_r[i]);
            if (!isfinite(x)) return 0;
            if (x > peak) peak = x;
        }
    }
    web_keys(0);
    return peak > 0.00001f && usr_nz[k] && smp_user_header_bound[k] && unchanged();
}
static int rejected_media(uint32_t which)
{
    fixture(which == 1);
    if (which == 0) { /* H2: tuning stays semantically valid but its CRC changes. */
        nor[SMP_USER_BASE + extent[0] * SMP_USER_SECTOR_SIZE + 52] &= ~0x40u;
    }
    if (which == 2) { /* C3: a valid-looking unanchored old fixed-slot header. */
        smp_user_hdr_t h; memcpy(&h, nor + SMP_USER_BASE + extent[0] * SMP_USER_SECTOR_SIZE, sizeof h);
        web_nor_erase(); st_prog(SMP_USER_BASE, &h, sizeof h);
    }
    memcpy(pool_before, nor + SMP_USER_BASE, sizeof pool_before); web_boot();
    if (!unchanged() || usr_nz[0] || smp_user_header_bound[0] || !smp_user_unresolved[0]) return 0;
    uint32_t p, n, cap;
    if (!smp_user_stage_begin(0, 256, &p, &n, &cap)) return 0;
    if (!which) return usr_nz[1] && usr_nz[2] && smp_alloc_test(extent[0]);
    if (smp_journal_state != SMP_JOURNAL_QUARANTINED) return 0;
    for (uint32_t s = 0; s < 60; s++) if (!smp_alloc_test(s)) return 0;
    return !usr_nz[1] && !usr_nz[2];
}
static int busy_boot(uint32_t unused)
{
    (void)unused; fixture(0); uint32_t writes = web_flash_writes;
    web_test_flash_busy(1);
    if (!(fl_status_ram() & 1u)) return 0;
    web_boot();
    if (flash_ok || smp_journal_state != SMP_JOURNAL_BLANK || web_flash_writes != writes || !unchanged()) return 0;
    for (uint32_t k = 0; k < 8; k++) if (usr_nz[k] || smp_user_header_bound[k]) return 0;
    web_test_flash_busy(0); web_boot(); /* one-shot boot must not silently reconcile */
    return !fl_status_ram() && !flash_ok && smp_journal_state == SMP_JOURNAL_BLANK && unchanged();
}
static int invalid_address(uint32_t off)
{
    (void)fm1_xip_ptr(off);
    return 0; /* must never reach here */
}
static void run(const char *name, int (*test)(uint32_t), uint32_t arg, int trap)
{
    fflush(NULL);
    pid_t pid = fork();
    if (pid == 0) {
        signal(SIGILL, SIG_DFL); signal(SIGTRAP, SIG_DFL);
        _exit(test(arg) ? 0 : 1);
    }
    int status = 0, ok = pid > 0 && waitpid(pid, &status, 0) == pid;
    ok &= trap ? WIFSIGNALED(status) && (WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP) : WIFEXITED(status) && WEXITSTATUS(status) == 0;
    printf("simulator XIP: %s %s\n", name, ok ? "ok" : "FAIL"); failures += !ok;
}
#ifndef SAMPLE_XIP_TEST_NO_MAIN
int main(void)
{
    struct rlimit limit = {0, 0}; setrlimit(RLIMIT_CORE, &limit);
    run("physical offsets map directly, including first/last NOR bytes", mapping, 0, 0);
    run("erased boot stays blank and retains eight public slots", erased_boot, 0, 0);
    run("v2 journal recovers relocated USR1-8 for all three audio decoders", valid_boot, 0, 0);
    for (uint32_t i = 0; i < 24; i++) {
        char name[80]; snprintf(name, sizeof name, "%s plays relocated USR%u through simulator audio", i / 8 == 0 ? "SAMPLE" : i / 8 == 1 ? "GRAIN" : "SLICE", i % 8 + 1);
        run(name, render_slot, i, 0);
    }
    run("corrupt v2 header is hidden without fallback or sector reuse", rejected_media, 0, 0);
    run("historical v1 publication quarantines the whole pool", rejected_media, 1, 0);
    run("unanchored FSMP stays quarantined", rejected_media, 2, 0);
    run("busy startup refuses recovery and leaves NOR untouched", busy_boot, 0, 0);
    run("one-past-NOR offset traps", invalid_address, sizeof nor, 1);
    run("UINT32_MAX offset traps", invalid_address, UINT32_MAX, 1);
    run("hardware CPU XIP address is not a physical flash offset", invalid_address, FM1_XIP(SMP_USER_BASE), 1);
    return failures ? 1 : 0;
}

#endif
