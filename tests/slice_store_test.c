/* SPDX-License-Identifier: GPL-3.0-only */
/* Regression for C1: dynamic allocations do not own the old fixed-slot tail.
 * Standalone, no generated tables or hardware: cc -std=c11 -Wall -Wextra
 * -Wno-unused-function tests/slice_store_test.c -o /tmp/slice_store_test */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/src/sample_storage.h"

/* Supply observable flash hooks and enough engine state to exercise the old
 * implementation too. Each byte, including pool-boundary canaries, is checked. */
#define SLC_AUTO 32u
static uint8_t flash[SMP_USER_POOL_SIZE + 2u * SMP_USER_SECTOR_SIZE];
static uint8_t before[sizeof flash];
static uint8_t usr_nz[SMP_USER_LOGICAL_SLOTS], slc_man_save;
static uint32_t phys[SMP_USER_LOGICAL_SLOTS];
static uint32_t reads, erases, programs, restores;
static int busy, page, failures;
static char message[32];
static uint8_t flash_ok = 1;
typedef struct { uint32_t data_len, crc; } smp_user_hdr_t;
typedef struct { uint32_t len; } slc_src_t;
typedef struct { uint32_t n, end, pos[SLC_AUTO]; } slc_man_t;
static slc_src_t source = {2048};
static slc_man_t manual = {2, 2048, {0, 1024}};
static const uint8_t *smp_user_xip(uint32_t k)
{
    reads++;
    return flash + SMP_USER_SECTOR_SIZE + phys[k] - SMP_USER_BASE;
}
static const slc_src_t *slc_get(uint32_t k) { (void)k; return &source; }
static const slc_man_t *slc_man_of(const slc_src_t *s) { (void)s; return &manual; }
static uint32_t st_crc32(const void *p, uint32_t n) { (void)p; (void)n; return 123; }
static void slc_man_restore(uint32_t k, uint32_t n, uint32_t end, const uint32_t *pos)
{ (void)k; (void)n; (void)end; (void)pos; restores++; }
static void (*slc_man_load)(uint32_t);
static int st_erase(uint32_t off)
{
    erases++;
    if (off < SMP_USER_BASE || off > SMP_USER_BASE + SMP_USER_POOL_SIZE - SMP_USER_SECTOR_SIZE)
        return -1;
    memset(flash + SMP_USER_SECTOR_SIZE + off - SMP_USER_BASE, 255, SMP_USER_SECTOR_SIZE);
    return 0;
}
static int st_prog(uint32_t off, const void *p, uint32_t n)
{
    programs++;
    if (off < SMP_USER_BASE || n > SMP_USER_POOL_SIZE || off - SMP_USER_BASE > SMP_USER_POOL_SIZE - n)
        return -1;
    memcpy(flash + SMP_USER_SECTOR_SIZE + off - SMP_USER_BASE, p, n);
    return 0;
}
static int transport_busy(void) { return busy; }
static int slice_page_on(void) { return page; }
static void ui_message(const char *s) { snprintf(message, sizeof message, "%s", s); }
#ifndef SLC_STORE_SOURCE
#define SLC_STORE_SOURCE "../firmware/src/slice_store.c"
#endif
#include SLC_STORE_SOURCE

static void check(const char *name, int pass)
{
    printf("slice store: %s %s\n", name, pass ? "ok" : "FAIL");
    failures += !pass;
}
static int untouched(void)
{
    return !reads && !erases && !programs && !restores && !memcmp(flash, before, sizeof flash);
}
int main(void)
{
    /* The former USR1 tail sector now belongs to USR2 or an upload. */
    memset(flash, 0xA5, sizeof flash);
    for (uint32_t k = 0; k < SMP_USER_LOGICAL_SLOTS; k++) {
        phys[k] = SMP_USER_BASE + SMP_USER_SECTOR_SIZE;
        usr_nz[k] = 1;
    }
    smp_user_hdr_t h = {1024, 123};
    memcpy(flash + 2u * SMP_USER_SECTOR_SIZE, &h, sizeof h);
    memcpy(before, flash, sizeof flash);
    slc_man_t keep = manual;
    check("relocated sample refuses persistence", slc_store_write(0) == 4);
    check("other sample / staging sector and pool canaries unchanged", untouched());
    if (failures) return 1;
    /* Near the pool end, the former metadata offset is outside the pool. */
    for (uint32_t k = 0; k < SMP_USER_LOGICAL_SLOTS; k++)
        phys[k] = SMP_USER_BASE + SMP_USER_POOL_SIZE - SMP_USER_SECTOR_SIZE;
    memcpy(flash + SMP_USER_SECTOR_SIZE + SMP_USER_POOL_SIZE - SMP_USER_SECTOR_SIZE, &h, sizeof h);
    memcpy(before, flash, sizeof flash);
    slc_store_boot();
    for (uint32_t k = 0; k < SMP_USER_LOGICAL_SLOTS; k++) slc_man_load(k);
    check("boot load never reads unowned or out-of-pool metadata", untouched());

    /* Legacy tails are also reclaimable; an old fixed base is not ownership. */
    for (uint32_t k = 0; k < SMP_USER_SLOTS; k++) phys[k] = SMP_USER_BASE + (k + 1u) * SMP_USER_SECTOR_SIZE;
    slc_man_save = (1u << SMP_USER_SLOTS) - 1u;
    busy = 1;
    slc_store_poll();
    check("playing defers notification", slc_man_save && !message[0] && untouched());
    busy = 0; page = 1;
    slc_store_poll();
    check("open page defers notification", slc_man_save && !message[0] && untouched());
    page = 0;
    slc_store_poll();
    check("legacy save explicitly reports RAM only", !slc_man_save && !strcmp(message, "SLICES RAM ONLY"));
    check("RAM edits and every flash byte preserved", !memcmp(&manual, &keep, sizeof keep) && untouched());
    message[0] = 0;
    slc_store_poll();
    check("no repeated notification", !message[0] && untouched());
    usr_nz[0] = 0; slc_man_save = 1;
    slc_store_poll();
    check("empty slot is a no-op", !slc_man_save && !message[0] && untouched());
    usr_nz[0] = 1; flash_ok = 0; slc_man_save = 1;
    slc_store_poll();
    check("no-flash mode also reports RAM only", !strcmp(message, "SLICES RAM ONLY") && untouched());
    return failures ? 1 : 0;
}
