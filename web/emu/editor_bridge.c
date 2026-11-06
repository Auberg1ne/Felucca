/* SPDX-License-Identifier: GPL-3.0-only */
/* S4A: complete-message transport for the ACTUAL editor, with OTA disabled.
 * Full RX: F0 7D 46 4C cmd [at most two arguments] F7 = 6..8 bytes.
 * One RX and one reserved TX; no USB ring, waits, scans, writes or WATCH. */
static uint8_t web_ed_input[8], web_ed_request[8], web_ed_output[664];
static uint32_t web_ed_epoch, web_ed_last_epoch, web_ed_ticket, web_ed_last_ticket, web_ed_pending, web_ed_len, web_ed_last_ms, web_ed_calls;
static int web_ed_room(void) { return !web_ed_len; }
static int web_ed_stop_guard(void) { web_editor_forbidden++; return 1; }
static void audio_silence(void) { web_editor_forbidden++; }
/* Do not even queue deferred persistence or extend its hold from this transport. */
static void web_ed_settings_save(void) { web_editor_forbidden++; }
static void web_ed_autosave_hold(void) { web_editor_forbidden++; }
static int fl_write(uint32_t off, const void *p, uint32_t n)
{ (void)off; (void)p; (void)n; web_editor_forbidden++; return -8; }
static int fl_erase4k(uint32_t off, uint32_t *took)
{ (void)off; *took = 0; web_editor_forbidden++; return -8; }
static void fl_inval(uint32_t off, uint32_t n)
{ (void)off; (void)n; web_editor_forbidden++; }
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    if (!web_editor_dispatching || web_ed_len || n > sizeof web_ed_output) {
        web_editor_forbidden++; return -1;
    }
    memcpy(web_ed_output, p, n); web_ed_len = n; return 0;
}
/* Never serviced: supplying these hooks does not enable the OTA/USB parser. */
static int ota_frame_get(const uint8_t **p, uint32_t *n) { (void)p; (void)n; return 0; }
static void ota_frame_done(void) { web_editor_forbidden++; }
#define settings_save web_ed_settings_save
#define autosave_hold web_ed_autosave_hold
#define ED_TRANSPORT_ROOM web_ed_room
#define ED_FLASH_STOP_GUARD web_ed_stop_guard
#define MENU_N (32u + FELUCCA_CPU2)
#include "../../firmware/src/editor.c"
#undef MENU_N
#undef ED_FLASH_STOP_GUARD
#undef ED_TRANSPORT_ROOM
#undef settings_save
#undef autosave_hold
_Static_assert(sizeof ed_out == sizeof web_ed_output, "review transport capacity if editor output changes");
static int web_ed_allowed(const uint8_t *p, uint32_t n)
{
    if (n < 6u || n > 8u || p[0] != 0xF0 || p[n-1] != 0xF7 ||
        p[1] != 0x7D || p[2] != 0x46 || p[3] != 0x4C) return 0;
    for (uint32_t i = 1; i + 1 < n; i++) if (p[i] >= 128) return 0;
    uint32_t a = n - 6u;
    switch (p[4]) {
    case 1: case 25: case 27: return a == 0; /* TRACK query ONLY */
    case 2: case 5:
        return a == 2 && ((p[5] == 0 && p[6] < P_COUNT) || (p[5] == 1 && p[6] < G_COUNT));
    case 10: return a == 1 && p[5] < NENGINES;
    case 29: return a == 1 && p[5] < NTRK;
    case 31: return a == 2 && p[5] < NTRK && p[6] < P_COUNT;
    default: return 0;
    }
}
/* Status: 0 accepted, 1 frame/policy denied, 2 occupied, 3 session/ticket invalid. */
static uint32_t web_ed_open(uint32_t epoch)
{
    if (!epoch || epoch <= web_ed_last_epoch || !web_booted || web_ed_epoch) return 3;
    web_ed_epoch = web_ed_last_epoch = epoch; web_ed_last_ticket = 0; return 0;
}
static void web_ed_cancel(uint32_t epoch, uint32_t ticket)
{
    if (epoch != web_ed_epoch || ticket != web_ed_ticket) return;
    web_ed_pending = web_ed_len = 0;
}
static void web_ed_close(uint32_t epoch)
{
    if (epoch != web_ed_epoch) return;
    web_ed_pending = web_ed_len = web_ed_epoch = 0;
}
static uint32_t web_ed_submit(uint32_t epoch, uint32_t ticket, uint32_t n)
{
    if (!epoch || epoch != web_ed_epoch || !ticket || ticket <= web_ed_last_ticket) return 3;
    if (!web_ed_allowed(web_ed_input, n)) return 1;
    if (web_ed_pending || web_ed_len) return 2;
    memcpy(web_ed_request, web_ed_input, n);
    web_ed_ticket = web_ed_last_ticket = ticket; web_ed_pending = n; return 0;
}
static void web_ed_step(void)
{
    if (!web_ed_pending || !web_ed_epoch || web_ed_len || fm1_ms - web_ed_last_ms < 15u) return;
    web_ed_last_ms = fm1_ms;
    /* Check again at dispatch; no ed_service(), ed_sync(), USB or updater calls. */
    uint32_t n = web_ed_pending; web_ed_pending = 0;
    if (!web_ed_allowed(web_ed_request, n)) return;
    web_editor_dispatching = 1; web_ed_calls++;
    ed_handle(web_ed_request + 1, n - 2);
    web_editor_dispatching = 0;
}
