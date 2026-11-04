/* SPDX-License-Identifier: GPL-3.0-only */
/* C2/C3 regression: actual allocator, upload and ERASE handlers on physical NOR.
 * cc -O1 -w -Ibuild/gen -Ifirmware/src
 *    tests/sample_delete_test.c -lm -o build/host/sample_delete_test
 * Uses the same flat-memory XIP rebasing as the existing sample host tests. */
#include <stdint.h>
#include <setjmp.h>
#include <assert.h>
#include "../firmware/hal/fm1_xip.h"
static uint8_t nor[0x100000], saved[sizeof nor];
static const uint8_t *host_sample_ptr(uint32_t off);
#define fm1_xip_ptr host_sample_ptr
#define FELUCCA_OTA 1
#define FELUCCA_FLASH 0
#define FELUCCA_VERSION "TEST"
#define FELUCCA_CDC 1                                    /* (as the firmware: MENU > USB SERIAL switches the console) */
#define main hostsim_main
#include "hostsim.c"
#undef main
#define MENU_N (32u + FELUCCA_CPU2)                  /* 1.6.1 (core.h FELUCCA_CPU2, the default): 2ND CORE, id 32 */

static uint32_t host_progress = 1;
static uint8_t host_wire[4096];
static uint32_t host_wire_n;
static void host_drain(void)
{
    while (so_r != so_w) {
        uint32_t p = sx_out_q[so_r++ % SXQ], cin = p & 15u, i;
        uint32_t n = cin == 4u || cin == 7u ? 3u : cin == 6u ? 2u : 1u;
        for (i = 0; i < n && host_wire_n < sizeof host_wire; i++)
            host_wire[host_wire_n++] = (uint8_t)(p >> (8u * (i + 1u)));
    }
}
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) { host_drain(); fm1_ms++; }
static void fm1_wdt_feed(void)
{
    fm1_ms++;
    if (host_progress && transport_req == 2u) { seq_stop(); transport_req = 0; }
}
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static void lcd_sync(void) {}
static void lcd_power(uint32_t s) { (void)s; }   /* (MENU > SCREEN OFF: lcd.c) */
static void lcd_wake_now(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/menu_items.c"
static void panel_setup(void) {}
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"

static uint32_t flash_ok = 1;
static void audio_silence(void) {}
static uint32_t bad_phys, pending_phys, pending_len;
static uint8_t bad_sector[4096], pending_data[32];
static int bad_read_active, silent_erase, fault_mode, committed_write;
static uint32_t fault_op, record_op, record_phys;
static uint32_t writes, write_off[8], write_len[8], erased_phys, erases, xip_reads;
static const uint8_t *host_sample_ptr(uint32_t off)
{
    assert(off < sizeof nor);
    xip_reads++;
    return bad_read_active && off == bad_phys ? bad_sector : nor + off;
}
static void fl_inval(uint32_t off, uint32_t n)
{
    if (fault_mode == 5 && committed_write && record_op == fault_op && off == record_phys && n == 32) {
        bad_phys = off; memcpy(bad_sector, nor + off, sizeof bad_sector);
        bad_sector[28] ^= 1u; bad_read_active = 1;
    }
}
static jmp_buf power_cut;
static int budget = -1, protected_write, erase_error, lost_ack;

static void tick_before(void) { if (budget == 0) longjmp(power_cut, 1); }
static void tick_after(void) { if (budget > 0 && --budget == 0) longjmp(power_cut, 1); }
static void bounds(uint32_t off, uint32_t n)
{
    assert(off >= SMP_USER_BASE && n <= SMP_USER_POOL_SIZE &&
           off - SMP_USER_BASE <= SMP_USER_POOL_SIZE - n);
}
static int fl_erase4k(uint32_t off, uint32_t *took)
{
    bounds(off, SMP_USER_SECTOR_SIZE); assert(!(off & 4095u));
    erased_phys = off; *took = 0; erases++;
    if (erase_error) return 1;
    if (silent_erase) return 0;
    for (uint32_t i = 0; i < 4096; i++) {
        tick_before(); nor[off + i] = 255; tick_after();
    }
    return 0;
}
static int fl_write(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *p = src;
    bounds(off, n);
    if (writes < 8) { write_off[writes] = off; write_len[writes] = n; }
    writes++;
    int body = n == 28 && off < SMP_USER_BASE + SMP_USER_SECTOR_SIZE;
    if (body) {
        memcpy(&record_op, src, 4); record_phys = off; committed_write = 0;
        if (record_op == fault_op && fault_mode == 1) return 1;
    }
    int marker = n == 4 && off == record_phys + 28;
    if (marker && record_op == fault_op && fault_mode == 4) {
        pending_phys = off; pending_len = n; memcpy(pending_data, src, n); return 1;
    }
    /* Same page splitting as fl_write in fm1_flash.h; no zero-to-one writes. */
    while (n) {
        uint32_t chunk = 256u - (off & 255u);
        if (chunk > n) chunk = n;
        for (uint32_t i = 0; i < chunk; i++) {
            tick_before();
            assert((nor[off] & *p) == *p);
            if (protected_write != 1 && !(protected_write == 2 && writes == 2)) nor[off] &= *p;
            off++; p++; tick_after();
        }
        n -= chunk;
    }
    if (marker) committed_write = 1;
    if (record_op == fault_op && ((fault_mode == 2 && body) || (fault_mode == 3 && marker))) return 1;
    return lost_ack && writes == (uint32_t)lost_ack ? 1 : 0;
}
static int st_read(uint32_t off, void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int st_prog(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int st_erase(uint32_t off) { (void)off; return -1; }
#include "../firmware/src/storage.c"
#include "../firmware/src/editor.c"
static int failures;
static void check(const char *s, int ok)
{ printf("sample delete: %s %s\n", s, ok ? "ok" : "FAIL"); failures += !ok; }
static void reboot(void)
{
    memset(ed_smp_stage_phys, 0, sizeof ed_smp_stage_phys);
    memset(ed_smp_stage_sectors, 0, sizeof ed_smp_stage_sectors);
    memset(ed_smp_stage_capacity, 0, sizeof ed_smp_stage_capacity);
    memset(ed_smp_stage_data_len, 0, sizeof ed_smp_stage_data_len);
    flash_ok = 1; bad_read_active = committed_write = 0;
    smp_user_alloc_boot();
    for (uint32_t k = 0; k < SMP_USER_SLOTS; k++) smp_user_scan(k);
}
static void fresh(void)
{
    memset(nor, 255, sizeof nor); budget = -1;
    protected_write = erase_error = lost_ack = silent_erase = fault_mode = 0;
    fault_op = record_op = 0;
    pending_len = 0; writes = erases = 0;
    song.playing = 0; transport_req = 0;
    reboot();
}
static uint32_t pack(const void *v, uint32_t n, uint8_t *out)
{
    const uint8_t *p = v; uint32_t at = 0;
    while (n) {
        uint32_t k = n > 7 ? 7 : n; uint8_t mask = 0;
        for (uint32_t j = 0; j < k; j++) mask |= (p[j] >> 7) << j;
        out[at++] = mask;
        for (uint32_t j = 0; j < k; j++) out[at++] = p[j] & 127;
        p += k; n -= k;
    }
    return at;
}
static smp_user_hdr_t header(uint8_t data)
{
    smp_user_hdr_t h = {0}; uint8_t payload[256]; memset(payload, data, sizeof payload);
    h.magic = SMP_USER_MAGIC; h.version = 1; h.nz = 1; h.data_len = 256;
    h.crc = st_crc32(payload, sizeof payload);
    h.zone[0].n = 512; h.zone[0].le = 511; h.zone[0].rate = 65536; h.zone[0].hi = 127;
    return h;
}
static int upload(uint32_t k, uint8_t data)
{
    uint8_t packed[600], payload[256]; smp_user_hdr_t h = header(data);
    memset(payload, data, sizeof payload);
    if (ed_smp_begin(k, sizeof payload)) return 1;
    if (fl_write(ed_smp_stage_phys[k] + SMP_USER_DATA, payload, sizeof payload)) return 1;
    return ed_smp_end(k, packed, pack(&h, sizeof h, packed));
}
static void legacy(uint32_t k, uint8_t data)
{
    uint32_t phys = SMP_USER_BASE + k * SMP_USER_SIZE;
    smp_user_hdr_t h = header(data);
    memcpy(nor + phys, &h, sizeof h);
    memset(nor + phys + SMP_USER_DATA, data, h.data_len);
}
static int gone(uint32_t k)
{ return smp_user_deleted[k] && !smp_user_phys[k] && !usr_nz[k]; }
static int erase_request(uint32_t k)
{
    const uint8_t frame[] = {ED_HDR0, ED_HDR1, ED_HDR2, ED_SMP_ERASE, (uint8_t)k};
    host_wire_n = 0; ed_handle(frame, sizeof frame); host_drain();
    return host_wire_n == 8 && host_wire[5] == k ? host_wire[6] : -1;
}
static int write_page(uint32_t k, uint32_t off, const uint8_t *page, uint32_t len)
{
    uint8_t f[640] = {ED_HDR0, ED_HDR1, ED_HDR2, ED_SMP_WRITE};
    f[4] = (uint8_t)k; f[5] = off & 127; f[6] = (off >> 7) & 127; f[7] = (off >> 14) & 127;
    host_wire_n = 0; ed_handle(f, 8 + pack(page, len, f + 8)); host_drain();
    return host_wire_n == 11 ? host_wire[9] : -1;
}
static void finish_late(void)
{
    for (uint32_t i = 0; i < pending_len; i++) {
        assert((nor[pending_phys + i] & pending_data[i]) == pending_data[i]);
        nor[pending_phys + i] &= pending_data[i];
    }
    pending_len = 0;
}
static int locked_mutations(uint32_t k)
{
    uint32_t w = writes, e = erases, p = ed_smp_stage_phys[k], c = smp_stage_count[k];
    uint8_t bits[sizeof smp_user_occupied], packed[600], payload[256] = {0};
    smp_user_hdr_t h = header(0x11); memcpy(bits, smp_user_occupied, sizeof bits);
    int ok = ed_smp_begin(k, 256) != 0 && erase_request(k) != 0 && ed_smp_erase(k) != 0 &&
        ed_smp_end(k, packed, pack(&h, sizeof h, packed)) != 0 && write_page(k, 512, payload, 256) != 0;
    ed_smp_stage_abort(k); smp_user_stage_abort(k);
    return ok && w == writes && e == erases && p == ed_smp_stage_phys[k] &&
        c == smp_stage_count[k] && !memcmp(bits, smp_user_occupied, sizeof bits);
}
static smp_alloc_desc_t fake_tomb(uint32_t slot, uint32_t gen)
{
    smp_alloc_desc_t d = {0}; d.magic = SMP_ALLOC_DELETE_MAGIC; d.version = 1;
    d.logical_slot = (uint8_t)slot; d.sector_count = 1; d.generation = gen;
    d.commit = SMP_ALLOC_COMMITTED; d.record_crc = st_crc32(&d, 28);
    return d;
}
static void payload_provenance(void)
{
    /* Exact original sequence, then other offsets, slots, and metadata magics. */
    for (uint32_t offset = 1; offset <= 4; offset++) {
        fresh(); assert(!upload(0, 0x11)); assert(!upload(2, 0x33)); reboot();
        uint32_t old0 = smp_user_phys[0], old2 = smp_user_phys[2];
        assert(!ed_smp_begin(1, 4 * 4096));
        smp_alloc_desc_t fake = fake_tomb(0, smp_user_generation[0] + 1);
        uint8_t page[256]; memset(page, 255, sizeof page); memcpy(page + 224, &fake, sizeof fake);
        check("unfinished slot-1 tombstone-like page is accepted only as audio", !write_page(1, offset * 4096 + 256, page, 256));
        reboot();
        check("reboot never deletes or releases committed slot 0", usr_nz[0] && smp_user_phys[0] == old0 &&
              !smp_user_deleted[0] && smp_alloc_test((old0 - SMP_USER_BASE) / 4096));
        check("cross-slot sample and unfinished ownership remain protected", usr_nz[2] && smp_user_phys[2] == old2 && !usr_nz[1]);
        uint32_t claims = 0; for (uint32_t s = 0; s < 60; s++) claims += smp_alloc_test(s);
        check("unfinished claims are not recycled on reboot", claims == 8);
    }
    /* A complete nested FSMP/SMA1 can also be arbitrary ADPCM bytes. */
    fresh(); assert(!upload(0, 0x11)); reboot(); uint32_t old = smp_user_phys[0];
    assert(!ed_smp_begin(1, 8192)); uint8_t nested[768]; memset(nested, 255, sizeof nested);
    smp_user_hdr_t h = header(0x22); memcpy(nested, &h, sizeof h); memset(nested + 512, 0x22, 256);
    smp_alloc_desc_t d = {0}; d.magic = SMP_ALLOC_MAGIC; d.version = 1; d.logical_slot = 0;
    d.sector_count = 1; d.generation = 99; d.data_len = 256; d.data_crc = h.crc;
    d.commit = SMP_ALLOC_COMMITTED; d.record_crc = st_crc32(&d, 28); memcpy(nested + 480, &d, sizeof d);
    for (uint32_t p = 0; p < sizeof nested; p += 256) assert(!write_page(1, 4096 + p, nested + p, 256));
    reboot(); check("complete SMA1 header embedded in unfinished audio has no authority", usr_nz[0] && smp_user_phys[0] == old && !usr_nz[1]);
    /* Journal-shaped payload is not in the anchored metadata sector. */
    assert(!ed_smp_begin(1, 8192)); d = fake_tomb(0, 99); d.magic = SMP_JOURNAL_DELETE;
    d.sector_count = 0; d.record_crc = 0; d.record_crc = st_crc32(&d, 28);
    uint8_t page[256]; memset(page, 255, sizeof page); memcpy(page, &d, sizeof d);
    assert(!write_page(1, 4096, page, 256)); reboot();
    check("SMJD bytes outside the protected root cannot delete a sample", usr_nz[0] && smp_user_phys[0] == old);
    assert(!ed_smp_begin(1, 8192)); d = smp_journal_header(); d.commit = SMP_ALLOC_COMMITTED;
    memset(page, 255, sizeof page); memcpy(page, &d, sizeof d); assert(!write_page(1, 4096, page, 256)); reboot();
    check("SMJ1-shaped audio cannot replace the fixed root", usr_nz[0] && smp_user_phys[0] == old && smp_journal_state == SMP_JOURNAL_READY);
    /* Also publish a complete, valid outer sample containing nested records. */
    assert(!ed_smp_begin(1, 8192));
    static uint8_t audio[8192]; memset(audio, 0x11, sizeof audio);
    d = fake_tomb(0, 0x7FFFFFFFu); memcpy(audio + 4096 + 480 - 512, &d, sizeof d);
    for (uint32_t p = 0; p < sizeof audio; p += 256) assert(!write_page(1, 512 + p, audio + p, 256));
    h = header(0x11); h.data_len = sizeof audio; h.crc = st_crc32(audio, sizeof audio);
    h.zone[0].n = sizeof audio * 2; h.zone[0].le = h.zone[0].n - 1;
    uint8_t packed[600]; assert(!ed_smp_end(1, packed, pack(&h, sizeof h, packed))); reboot();
    check("published audio still grants no authority to its nested tombstone", usr_nz[1] && usr_nz[0] && smp_user_phys[0] == old);
    assert(!ed_smp_begin(1, 256)); writes = 0;
    check("client cannot address the journal through WRITE", write_page(1, 0, page, 256) != 0 &&
          write_page(1, 256, page, 256) != 0 && write_page(1, 0x1FFF00, page, 256) != 0 && !writes);
    fresh(); assert(!ed_smp_journal_ready()); smp_alloc_release(0); writes = erases = 0;
    check("allocator cannot return the root even if its bitmap bit is cleared", ed_smp_begin(0, 256) != 0 && !writes && !erases);
}
static void fault_tests(void)
{
    const uint32_t ops[] = {SMP_JOURNAL_CLAIM, SMP_JOURNAL_PUBLISH, SMP_JOURNAL_DELETE};
    for (uint32_t op = 0; op < 3; op++) for (int mode = 1; mode <= 5; mode++) {
        fresh(); assert(!upload(0, 0x11)); reboot(); uint32_t old = smp_user_phys[0];
        if (op == 1) { assert(!ed_smp_erase(0)); reboot(); }
        fault_op = ops[op]; fault_mode = mode;
        int rc = op == 0 ? ed_smp_begin(1, 256) : op == 1 ? upload(0, 0x22) : ed_smp_erase(0);
        check("uncertain journal operation fails and disables storage", rc != 0 && !flash_ok && smp_user_io_blocked);
        check("repeated ERASE, WRITE, abort and allocations retain uncertainty", locked_mutations(op == 0 ? 1 : 0) && locked_mutations(2));
        if (mode == 4) finish_late();
        fault_mode = 0; bad_read_active = 0; reboot();
        if (mode <= 2) {
            check("torn journal tail is quarantined without any sector reuse", smp_journal_state == SMP_JOURNAL_LOCKED ||
                  (mode == 1 && smp_journal_state == SMP_JOURNAL_READY));
        } else if (op == 0) check("recovered claim never publishes payload", usr_nz[0] && smp_user_phys[0] == old && !usr_nz[1]);
        else if (op == 1) {
            check("reboot reconciles a committed but unacknowledged publish", usr_nz[0] && !smp_user_deleted[0]);
            assert(!ed_smp_erase(0)); reboot(); check("ERASE after publish reconciliation stays durable", gone(0));
        } else check("reboot reconciles a committed but unacknowledged deletion", gone(0));
        check("original sample bytes survive the failed operation", nor[old + 512] == 0x11);
    }
    fresh(); assert(!ed_smp_begin(0, 256)); writes = 0; lost_ack = 1;
    uint8_t page[256]; memset(page, 0x11, sizeof page);
    check("payload I/O errors retain claim and block further mutation", write_page(0, 512, page, 256) != 0 && locked_mutations(0));
}
static void power_tests(void)
{
    /* DELETE is append-only: 28 body bytes + 4 commit bytes, no erase. */
    fresh(); assert(!upload(0, 0x11)); reboot(); uint32_t old = smp_user_phys[0]; memcpy(saved, nor, sizeof nor);
    int ok = 1;
    for (int cut = 0; cut <= 32; cut++) {
        memcpy(nor, saved, sizeof nor); budget = -1; reboot(); budget = cut;
        if (!setjmp(power_cut)) (void)ed_smp_erase(0);
        budget = -1; reboot();
        ok &= cut == 32 ? gone(0) : (usr_nz[0] || smp_journal_state == SMP_JOURNAL_LOCKED);
        ok &= !memcmp(nor + old, saved + old, 4096) && smp_alloc_test((old - SMP_USER_BASE) / 4096);
    }
    check("33 deletion power cuts preserve data and never reuse old extents", ok);
    /* A claim commits before its data sector is erased. */
    fresh(); assert(!upload(0, 0x11)); reboot(); old = smp_user_phys[0]; memcpy(saved, nor, sizeof nor); ok = 1;
    for (int cut = 0; cut <= 32 + 4096; cut++) {
        memcpy(nor, saved, sizeof nor); budget = -1; reboot(); budget = cut;
        if (!setjmp(power_cut)) (void)ed_smp_begin(1, 256);
        budget = -1; reboot();
        ok &= !usr_nz[1] && (usr_nz[0] || smp_journal_state == SMP_JOURNAL_LOCKED) &&
              !memcmp(nor + old, saved + old, 4096) && smp_alloc_test((old - SMP_USER_BASE) / 4096);
    }
    check("4129 claim/erase power cuts never expose uncommitted audio", ok);
    /* SMA1 plus journal PUBLISH; only the latter grants recovery authority. */
    fresh(); assert(!upload(0, 0x11)); reboot(); old = smp_user_phys[0];
    assert(!ed_smp_begin(0, 256)); uint8_t data[256], packed[600]; memset(data, 0x22, 256);
    assert(!write_page(0, 512, data, 256)); smp_user_hdr_t h = header(0x22); uint32_t n = pack(&h, sizeof h, packed);
    uint32_t stage = ed_smp_stage_phys[0], count = ed_smp_stage_sectors[0]; memcpy(saved, nor, sizeof nor); ok = 1;
    for (int cut = 0; cut <= 544; cut++) {
        memcpy(nor, saved, sizeof nor); budget = -1; reboot();
        /* Resume the known in-session stage handle for fault injection only. */
        ed_smp_stage_phys[0] = stage; ed_smp_stage_sectors[0] = count; ed_smp_stage_capacity[0] = count * 4096;
        ed_smp_stage_data_len[0] = 256; smp_stage_start[0] = (stage - SMP_USER_BASE) / 4096; smp_stage_count[0] = count;
        budget = cut;
        if (!setjmp(power_cut)) (void)ed_smp_end(0, packed, n);
        budget = -1; reboot();
        ok &= cut == 544 ? (smp_user_phys[0] == stage) :
            (smp_user_phys[0] == old || smp_journal_state == SMP_JOURNAL_LOCKED);
        ok &= !memcmp(nor + old, saved + old, 4096) && smp_alloc_test((old - SMP_USER_BASE) / 4096);
    }
    check("545 publish power cuts never authorize SMA1 without journal commit", ok);
    ok = 1;
    for (int cut = 0; cut <= 32; cut++) {
        fresh(); budget = cut;
        if (!setjmp(power_cut)) (void)ed_smp_journal_ready();
        budget = -1; reboot();
        ok &= !usr_nz[0] && !usr_nz[1] && !usr_nz[2];
        ok &= cut == 0 ? smp_journal_state == SMP_JOURNAL_BLANK :
            cut == 32 ? smp_journal_state == SMP_JOURNAL_READY : smp_journal_state == SMP_JOURNAL_LOCKED;
    }
    check("33 anchor power cuts cannot manufacture a sample or free occupied data", ok);
}
/* C3: no raw byte pattern is sufficient authority to publish a sample. */
static int quarantined(void)
{
    int ok = smp_journal_state == SMP_JOURNAL_QUARANTINED;
    for (uint32_t k = 0; k < SMP_USER_LOGICAL_SLOTS; k++) {
        smp_user_scan(k);
        ok &= !smp_user_phys[k] && !smp_user_sectors[k] && !usr_nz[k] && smp_user_unresolved[k];
        for (uint32_t z = 0; z < 16; z++) ok &= !usr_zone[k][z].n;
    }
    for (uint32_t s = 0; s < 60; s++) ok &= smp_alloc_test(s);
    return ok;
}
static int no_exports(void)
{
    uint32_t reads = xip_reads;
    uint8_t info[] = {ED_HDR0, ED_HDR1, ED_HDR2, ED_SMP_INFO};
    host_wire_n = 0; ed_handle(info, sizeof info); host_drain();
    int ok = host_wire_n == 8 + SMP_USER_SLOTS * 3;
    for (uint32_t i = 7; i + 1 < host_wire_n; i++) ok &= host_wire[i] == 0;
    for (uint32_t k = 0; k < SMP_USER_SLOTS; k++) {
        uint32_t len = 999;
        ok &= ed_bk_object(32 + k, &len) == 0 && len == 0;
        uint8_t get[] = {ED_HDR0, ED_HDR1, ED_HDR2, ED_BACKUP_GET, 32 + k, 0, 0, 0, 0, 0, 16, 0};
        ed_bk_valid = 1; ed_bk_usb = usb.resets;
        host_wire_n = 0; ed_handle(get, sizeof get); host_drain();
        ok &= host_wire_n > 7 && host_wire_n < 24 && host_wire[6] != 0;
    }
    return ok && reads == xip_reads;
}
static void quarantine_tests(void)
{
    fresh(); legacy(0, 0x11); legacy(1, 0x22); legacy(2, 0x33);
    smp_alloc_desc_t tomb = fake_tomb(0, 8);
    memcpy(nor + SMP_USER_BASE + 6 * 4096 + 480, &tomb, sizeof tomb);
    memcpy(saved, nor, sizeof nor); reboot();
    check("C3 genuine legacy images quarantined without publication", quarantined());
    check("C3 INFO and backup expose no quarantined content", no_exports());
    int ok = 1;
    for (uint32_t boot = 0; boot < 3; boot++) {
        reboot(); ok &= quarantined();
        for (uint32_t k = 0; k < SMP_USER_LOGICAL_SLOTS; k++) {
            /* Even a stale live handle cannot bypass the mutation gates. */
            ed_smp_stage_phys[k] = SMP_USER_BASE + 4096;
            ed_smp_stage_sectors[k] = smp_stage_count[k] = 1;
            ed_smp_stage_capacity[k] = 4096;
            ok &= locked_mutations(k);
        }
    }
    check("C3 repeated boots and stale-handle mutations never write", ok && !writes && !erases && !memcmp(saved, nor, sizeof nor));
    project_t project;
    memset(&project, 0, sizeof project);
    project.magic = PROJ_MAGIC; project.size = sizeof project;
    project.parts = NPART; project.phys = PROJ_PHYS; chain_defaults(&project.chain);
    for (uint32_t k = 0; k < NTRK; k++) {
        uint32_t eng = k == 1 ? ENGI_GRAIN : ENGI_SAMPLE;
#if FELUCCA_SLICE
        if (k == 2) eng = 13;
#endif
        project.t[k].engine = eng;
        for (uint32_t p = 0; p < P_COUNT; p++) project.t[k].p[p] = param_desc_of(eng, p)->def;
        project.t[k].p[P_E0] = eng == 13 ? 1 : SMP_NSETS;
        memcpy(project.fm6[k], FM6_INIT, FM6_PACKED);
    }
    project.sum = proj_sum(&project);
    ok = !project_restore_runtime(&project);
    for (uint32_t k = 0; k < NTRK; k++) {
        uint32_t code;
        ok &= snd_missing(&trk[k], &code) != 0;
    }
    check("C3 project restore cannot authorize quarantined engine sources", ok && quarantined() && !writes && !erases);
    /* Completed old header, cut before any SMA1 byte: indistinguishable from legacy.
     * Check every byte-prefix cut through header, descriptor body and COMT. */
    uint8_t image[512]; smp_user_hdr_t h = header(0x22);
    memset(image, 255, sizeof image); memcpy(image, &h, sizeof h);
    smp_alloc_desc_t d = {0}; d.magic = SMP_ALLOC_MAGIC; d.version = 1; d.logical_slot = 7;
    d.sector_count = 1; d.generation = 9; d.data_len = 256; d.data_crc = h.crc;
    d.commit = SMP_ALLOC_COMMITTED; d.record_crc = st_crc32(&d, 28);
    memcpy(image + 480, &d, sizeof d);
    ok = 1;
    for (uint32_t boundary = 0; boundary < 3; boundary++) {
        for (uint32_t cut = 0; cut <= sizeof image; cut++) {
            fresh(); uint32_t phys = SMP_USER_BASE + boundary * SMP_USER_SIZE;
            memset(nor + phys + 512, 0x22, 256); memcpy(nor + phys, image, cut);
            memcpy(saved, nor, sizeof nor); reboot();
            ok &= (smp_journal_state == SMP_JOURNAL_QUARANTINED || smp_journal_state == SMP_JOURNAL_LOCKED) &&
                !smp_user_phys[boundary] && !usr_nz[boundary] && !smp_user_phys[7] &&
                !writes && !erases && !memcmp(saved, nor, sizeof nor);
            for (uint32_t s = 0; s < 60; s++) ok &= smp_alloc_test(s);
        }
    }
    check("C3 all legacy boundaries and 513 header/descriptor cuts fail closed", ok);
    /* Complete counterfeit samples in an unfinished outer upload. */
    ok = 1;
    for (uint32_t k = 0; k < 8; k++) {
        fresh(); legacy(0, 0x11);
        d.logical_slot = k; d.generation = k & 1 ? 0x80000000u : 0xFFFFFFFFu;
        d.record_crc = 0; d.record_crc = st_crc32(&d, 28);
        memcpy(image + 480, &d, sizeof d);
        for (uint32_t sector = 2; sector <= 6; sector += 2) {
            memcpy(nor + SMP_USER_BASE + sector * 4096, image, sizeof image);
            memset(nor + SMP_USER_BASE + sector * 4096 + 512, 0x22, 256);
        }
        memcpy(saved, nor, sizeof nor); reboot();
        ok &= quarantined() && no_exports() && !memcmp(saved, nor, sizeof nor) && !writes && !erases;
    }
    check("C3 counterfeit committed SMA1 across slots and sector offsets stays quarantined", ok);
    /* Defense in depth: consumer gates also reject stale RAM publication state. */
    usr_nz[0] = 1; smp_user_phys[0] = SMP_USER_BASE; smp_user_sectors[0] = 1;
    check("C3 export guards reject stale published metadata", no_exports());
    smp_user_scan(0);
    check("C3 scan clears stale SAMPLE GRAIN and SLICE publication", !usr_nz[0] && !usr_zone[0][0].n &&
        smp_set_missing(SMP_NSETS) && !gr_nz(SMP_NSETS)
#if FELUCCA_SLICE
        && !slc_get(1)
#endif
    );
}
static void anchored_visibility_tests(void)
{
    fresh();
    int ok = smp_journal_state == SMP_JOURNAL_BLANK;
    for (uint32_t k = 0; k < 8; k++) ok &= !upload(k, 0x11 + k);
    reboot();
    for (uint32_t k = 0; k < 8; k++) {
        smp_user_scan(k);
        ok &= usr_nz[k] == 1 && usr_zone[k][0].n == 512 && !smp_user_unresolved[k] &&
            smp_user_phys[k] >= SMP_USER_BASE + 4096 && smp_alloc_test((smp_user_phys[k] - SMP_USER_BASE) / 4096) &&
            *fm1_xip_ptr(smp_user_phys[k] + 512) == 0x11 + k;
    }
    track_t track = {0}; voice_t voice = {0};
    track.p[P_E0] = SMP_NSETS; voice.note = 60;
    sample_note_on(&track, &voice);
    uint32_t len = 0;
    const uint8_t *backup = ed_bk_object(32, &len);
    ok &= !voice.s[6] && voice.s[4] == 0x8000 && gr_nz(SMP_NSETS) == 1 &&
        backup == fm1_xip_ptr(smp_user_phys[0]) && len == 768;
#if FELUCCA_SLICE
    ok &= slc_get(1) != 0;
#endif
    check("C3 anchored publication for all eight slots retains engine and backup access", ok && smp_journal_state == SMP_JOURNAL_READY);
}
static int all_reserved(void)
{
    for (uint32_t s = 0; s < SMP_USER_SECTOR_COUNT; s++) if (!smp_alloc_test(s)) return 0;
    return 1;
}
static void seal_record(smp_alloc_desc_t *d)
{ d->commit = SMP_ALLOC_COMMITTED; d->record_crc = 0; d->record_crc = st_crc32(d, 28); }
static void journal_rejection_tests(void)
{
    const uint32_t generations[] = {0, 1, 3, 0x80000001u, 0xFFFFFFFFu};
    for (uint32_t i = 0; i < sizeof generations / sizeof generations[0]; i++) {
        fresh(); assert(!upload(0, 0x11)); reboot();
        uint32_t old = smp_user_phys[0]; smp_alloc_desc_t d = {0};
        d.magic = SMP_JOURNAL_CLAIM; d.version = 1; d.logical_slot = 0; d.sector_count = 1;
        d.generation = generations[i]; d.data_len = 256; d.reserved = 2; seal_record(&d);
        memcpy(nor + SMP_USER_BASE + smp_journal_next, &d, sizeof d); reboot();
        check("duplicate, skipped, half-range or wrapped journal generations fail closed", smp_journal_state == SMP_JOURNAL_LOCKED &&
              all_reserved() && !usr_nz[0] && nor[old + 512] == 0x11 && locked_mutations(0));
    }
    for (int mode = 0; mode < 10; mode++) {
        fresh(); assert(!upload(0, 0x11)); reboot(); uint32_t old = smp_user_phys[0];
        smp_alloc_desc_t d = {0}; d.magic = SMP_JOURNAL_CLAIM; d.version = 1;
        d.logical_slot = 1; d.sector_count = 1; d.generation = 1; d.data_len = 256; d.reserved = 2;
        if (mode == 0) d.reserved = 1;                   /* overlaps existing claim */
        if (mode == 1) d.magic = SMP_JOURNAL_PUBLISH;    /* no preceding claim */
        if (mode == 2) d.reserved = 0;                   /* tries to claim root */
        if (mode == 6) d.reserved = SMP_USER_SECTOR_COUNT;
        if (mode == 7) d.sector_count = SMP_USER_SECTOR_COUNT;
        if (mode == 8) d.data_len = SMP_USER_SECTOR_SIZE;
        if (mode == 9) d.version = 2;
        seal_record(&d);
        if (mode == 3) d.record_crc ^= 1;
        if (mode == 4) d.commit ^= 1;
        uint32_t off = smp_journal_next + (mode == 5 ? 32 : 0); /* forbidden hole */
        memcpy(nor + SMP_USER_BASE + off, &d, sizeof d); reboot();
        check("rejected metadata never releases any previously occupied sector", smp_journal_state == SMP_JOURNAL_LOCKED &&
              all_reserved() && !usr_nz[0] && nor[old + 512] == 0x11 && locked_mutations(0));
    }
    fresh(); assert(!upload(0, 0x11)); reboot(); nor[SMP_USER_BASE + 20] ^= 1; reboot();
    check("invalid anchor hides samples and quarantines entire pool", smp_journal_state == SMP_JOURNAL_LOCKED && all_reserved());
    fresh(); assert(!upload(0, 0x11)); reboot(); uint32_t old = smp_user_phys[0];
    for (uint32_t k = 0; k < 59; k++) {
        int rc = ed_smp_begin(1, 256);
        if (rc) break;
        ed_smp_stage_abort(1);
    }
    writes = 0;
    check("aborted claims exhaust space safely without recycling it", ed_smp_begin(2, 256) != 0 && !writes && all_reserved());
    check("deletion works with no free audio sector", !ed_smp_erase(0) && nor[old + 512] == 0x11);
    reboot(); check("full-pool deletion remains durable", gone(0) && all_reserved());
    fresh();
    for (uint32_t i = 0; i < 60; i++) {
        if (upload(0, 0x11)) break;
        assert(!ed_smp_erase(0));
    }
    /* 42 CLAIM/PUBLISH/DELETE cycles use 126 of 127 journal records. */
    writes = 0;
    check("journal capacity refuses a new upload without programming", ed_smp_begin(1, 256) != 0 && !writes);
    assert(!ed_smp_erase(1)); writes = 0;
    check("full journal refuses deletion without erasing data", ed_smp_erase(2) != 0 && !writes);
    reboot(); check("full journal remains readable with durable deletions", gone(0) && gone(1) && smp_journal_state == SMP_JOURNAL_READY);
}
int main(void)
{
    fresh(); assert(!upload(0, 0x11)); reboot(); uint32_t old = smp_user_phys[0]; smp_zone_t zone = usr_zone[0][0];
    assert(!upload(0, 0x22)); uint32_t gen = smp_user_generation[0]; memcpy(saved, nor, sizeof nor); erases = 0;
    check("ERASE before reboot deletes the pending replacement", !erase_request(0) && smp_user_generation[0] == gen + 1);
    check("deletion never erases audio or changes retained zones", !erases &&
          !memcmp(&zone, &usr_zone[0][0], sizeof zone) && !memcmp(nor + SMP_USER_BASE + 4096, saved + SMP_USER_BASE + 4096, SMP_USER_POOL_SIZE - 4096));
    reboot(); check("original C2 failure 1 is prevented", gone(0));
    reboot(); check("deletion survives repeated reboots", gone(0));
    writes = 0; check("repeated ERASE is idempotent", !erase_request(0) && !writes);
    assert(!upload(0, 0x33)); reboot(); check("new publication supersedes deletion through journal order", usr_nz[0]);
    assert(!ed_smp_erase(0)); reboot(); check("original C2 failure 2 is prevented", gone(0));
    check("all old extents remain reserved after delete and reboot", smp_alloc_test((old - SMP_USER_BASE) / 4096));
    fresh(); assert(!ed_smp_begin(0, 256)); assert(!ed_smp_erase(0));
    uint8_t packed[600]; smp_user_hdr_t h = header(0x11);
    check("ERASE cancels staging and END cannot revive it", !ed_smp_stage_sectors[0] && ed_smp_end(0, packed, pack(&h, sizeof h, packed)) != 0);
    reboot(); check("deletion of an unfinished slot is durable", gone(0));
    payload_provenance(); fault_tests(); power_tests(); quarantine_tests(); anchored_visibility_tests(); journal_rejection_tests();
    return failures ? 1 : 0;
}
