/* SPDX-License-Identifier: GPL-3.0-only */
/* Actual storage backend: a latched sample-I/O fault must also stop callers
 * such as a previously started FM6 transfer. No hardware is accessed. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned calls;
static int32_t abuf[4];
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static void fm1__led_lines(uint32_t v) { (void)v; }
#define FL_FAR(fn) (fn)
#define FL_STORE_OK(off, n) ((off) >= 0x93000u && (n) <= 0x6D000u && (off) - 0x93000u <= 0x6D000u - (n))
static int fl_read_ram(uint32_t off, uint8_t *dst, uint32_t n)
{ (void)off; calls++; memset(dst, 0x55, n); return 0; }
static int fl_erase4k_ram(uint32_t off, uint32_t *took)
{ (void)off; calls++; *took = 0; return 0; }
static int fl_write(uint32_t off, const void *p, uint32_t n)
{ (void)off; (void)p; (void)n; calls++; return 0; }
#include "../firmware/src/storage_hw.c"
static int failures;
static void check(const char *s, int ok)
{ printf("storage fault gate: %s %s\n", s, ok ? "ok" : "FAIL"); failures += !ok; }
int main(void)
{
    uint8_t b[512] = {0}; flash_ok = 1;
    check("ready backend reads in bounded chunks", !st_read(0x93000, b, sizeof b) && calls == 2 && b[0] == 0x55);
    check("ready backend erases", !st_erase(0x93000) && calls == 3);
    check("ready backend programs", !st_prog(0x93000, b, sizeof b) && calls == 4);
    flash_ok = 0; memset(b, 0x33, sizeof b);
    check("latched fault blocks reads without changing destination", st_read(0x93000, b, sizeof b) != 0 && calls == 4 && b[0] == 0x33);
    check("latched fault blocks erase from an active caller", st_erase(0x93000) != 0 && calls == 4);
    check("latched fault blocks program from an active caller", st_prog(0x93000, b, sizeof b) != 0 && calls == 4);
    return failures ? 1 : 0;
}
