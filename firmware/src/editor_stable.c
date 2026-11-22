/* SPDX-License-Identifier: GPL-3.0-only */
/* Main-loop stable-ID transport. Legacy118 and negotiated native119 views are
 * distinct. C is scalar/storage only; there is no extended bulk replacement. */
#define ED_STABLE_COUNT 119u
#define ED_STABLE_FLAGS 23u /* GET, SET, SNAPSHOT, GLOBALS; never bulk replacement */
_Static_assert(P_COUNT == ED_STABLE_COUNT && P_E0 == 110u, "review stable/legacy publication before parameter growth");
static struct { uint32_t resets, ms; uint16_t nonce, count; } ed_stable;
/* A future native schema must fail closed until its legacy-loss policy is
 * explicitly integrated. Counts here are coverage, never layout provenance. */
static int ed_legacy_coverage(uint32_t required, uint32_t represented)
{
    return represented == ED_LEGACY_COUNT && required <= represented;
}
/* Legacy arrays cannot represent C. Reads/replacement must not silently drop it. */
static int ed_legacy_live(const track_t *t)
{
    return ed_legacy_coverage(t->p[P_GRC] ? P_COUNT : ED_LEGACY_COUNT, ED_LEGACY_COUNT);
}
static int ed_legacy_slot(uint32_t k)
{
    if (k >= UP_SLOTS) return 0;
#if FELUCCA_FLASH
    if (!up_legacy_c_ok(k)) return 0;
#endif
    const up_rec_t *r = up_rec(k);
    return !r->used || (up_valid(r) && (r->ver < UP_VER || up_value(r, P_GRC) == 0));
}
static void ed_stable_expire(void)
{
    if (ed_stable.nonce && (!usb.config || usb.resets != ed_stable.resets ||
        (uint32_t)(fm1_ms - ed_stable.ms) >= 5000u)) ed_stable.nonce = 0;
}
static int ed_stable_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    if (cmd != ED_STABLE) return 0;
    uint32_t op = n ? a[0] : 127u, rc = 0, nonce = n >= 5u ? a[3] | (uint32_t)a[4] << 7 : 0;
    ed_stable_expire();
    if (op == 0u) ed_stable.nonce = 0; /* every negotiation attempt replaces the previous session */
    if (n > 10u) rc = 1;
    for (uint32_t i = 0; i < n && i < 10u; i++) if (a[i] > 127u) rc = 1;
    if (n < 5u) rc = 1;
    else if (a[1] != 1u || a[2] != 1u) rc = 3;
    else if (!nonce) rc = 1;
    if (!rc && op == 0u) {
        if (n != 7u) rc = 1;
        else if ((a[5] | (uint32_t)a[6] << 7) != ED_STABLE_COUNT && (a[5] | (uint32_t)a[6] << 7) != ED_LEGACY_COUNT) rc = 3;
        else if (!usb.config) rc = 2;
        else { ed_stable.nonce = (uint16_t)nonce; ed_stable.count = (uint16_t)(a[5] | (uint32_t)a[6] << 7); ed_stable.resets = usb.resets; ed_stable.ms = fm1_ms; }
    } else if (!rc) {
        if (!usb.config || nonce != ed_stable.nonce || usb.resets != ed_stable.resets ||
            (uint32_t)(fm1_ms - ed_stable.ms) >= 5000u) rc = 2;
        else if (op == 1u || op == 2u) {
            if (n != (op == 1u ? 8u : 10u)) rc = 1;
            else if (a[5] >= NTRK || (a[6] | (uint32_t)a[7] << 7) >= ed_stable.count) rc = 4;
            else if (op == 2u) {
                track_t *t = &trk[a[5]]; uint32_t id = a[6] | (uint32_t)a[7] << 7;
                const param_desc_t *d = ed_tdesc(t, id); int32_t v = ed_rv(a + 8);
                if (d->max <= d->min || v < d->min || v > d->max) rc = 4;
                else if (chain_busy() && chain_owns(id)) rc = 5;
                else {
                    t->p[id] = (int16_t)enum_orig(d, v);
                    (void)motion_capture(t, id, t->p[id]); ed_known(a[5], id); ui.force = 1;
                }
            }
        } else if (op == 3u) {
            if (n != 6u) rc = 1;
            else if (a[5] >= NTRK) rc = 4;
            else if (ed_stable.count == ED_LEGACY_COUNT && !ed_legacy_live(&trk[a[5]])) rc = 3;
        } else if (op == 5u) {
            if (n != 5u) rc = 1;
            else if (ed_stable.count != ED_STABLE_COUNT) rc = 3;
        } else rc = 3; /* no bulk mutation or unknown operation */
    }
    ed_b(op); ed_b(rc); ed_b(1); ed_b(1); ed_b(nonce); ed_b(nonce >> 7);
    if (!rc && op == 0u) { ed_b(ed_stable.count); ed_b(ed_stable.count >> 7); ed_b(ed_stable.count == ED_LEGACY_COUNT ? 7u : ED_STABLE_FLAGS); }
    if (!rc && (op == 1u || op == 2u)) {
        uint32_t id = a[6] | (uint32_t)a[7] << 7;
        ed_b(a[5]); ed_b(id); ed_b(id >> 7); ed_v(motion_base_value(&trk[a[5]], id));
    }
    if (!rc && op == 3u) {
        track_t *t = &trk[a[5]]; ed_b(a[5]); ed_b(ed_eng(t)); ed_b(t->preset);
        ed_b(ed_stable.count); ed_b(ed_stable.count >> 7);
        for (uint32_t i = 0; i < ed_stable.count; i++) ed_v(motion_base_value(t, i));
    }
    if (!rc && op == 5u) {
        ed_b(G_COUNT); ed_b(G_COUNT >> 7);
        for (uint32_t i = 0; i < G_COUNT; i++) ed_v(song.g[i]);
    }
    return 1;
}
