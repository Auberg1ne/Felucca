/* SPDX-License-Identifier: GPL-3.0-only */
/* Main-loop stable-ID transport. This version implements only existing IDs.
 * No persistent value, MIDI encoding or storage schema is introduced here. */
#define ED_STABLE_COUNT 118u
#define ED_STABLE_FLAGS 7u /* GET, SET, SNAPSHOT; NOT bulk replacement or C */
_Static_assert(P_COUNT == ED_STABLE_COUNT && P_E0 == 110u, "review stable/legacy publication before parameter growth");
static struct { uint32_t resets, ms; uint16_t nonce; } ed_stable;
/* A future native schema must fail closed until its legacy-loss policy is
 * explicitly integrated. Counts here are coverage, never layout provenance. */
static int ed_legacy_coverage(uint32_t required, uint32_t represented)
{
    return represented == ED_LEGACY_COUNT && required <= represented;
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
        else if ((a[5] | (uint32_t)a[6] << 7) != ED_STABLE_COUNT) rc = 3;
        else if (!usb.config) rc = 2;
        else { ed_stable.nonce = (uint16_t)nonce; ed_stable.resets = usb.resets; ed_stable.ms = fm1_ms; }
    } else if (!rc) {
        if (!usb.config || nonce != ed_stable.nonce || usb.resets != ed_stable.resets ||
            (uint32_t)(fm1_ms - ed_stable.ms) >= 5000u) rc = 2;
        else if (op == 1u || op == 2u) {
            if (n != (op == 1u ? 8u : 10u)) rc = 1;
            else if (a[5] >= NTRK || (a[6] | (uint32_t)a[7] << 7) >= ED_STABLE_COUNT) rc = 4;
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
        } else rc = 3; /* no bulk mutation, no C, no unknown operation */
    }
    ed_b(op); ed_b(rc); ed_b(1); ed_b(1); ed_b(nonce); ed_b(nonce >> 7);
    if (!rc && op == 0u) { ed_b(ED_STABLE_COUNT); ed_b(ED_STABLE_COUNT >> 7); ed_b(ED_STABLE_FLAGS); }
    if (!rc && (op == 1u || op == 2u)) {
        uint32_t id = a[6] | (uint32_t)a[7] << 7;
        ed_b(a[5]); ed_b(id); ed_b(id >> 7); ed_v(motion_base_value(&trk[a[5]], id));
    }
    if (!rc && op == 3u) {
        track_t *t = &trk[a[5]]; ed_b(a[5]); ed_b(ed_eng(t)); ed_b(t->preset);
        ed_b(ED_STABLE_COUNT); ed_b(ED_STABLE_COUNT >> 7);
        for (uint32_t i = 0; i < ED_STABLE_COUNT; i++) ed_v(motion_base_value(t, i));
    }
    return 1;
}
