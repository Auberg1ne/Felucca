/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Per-sample decoder loops derived from eng_grain.c. */
#pragma once
/* Test controls only. Set before playback; method changes during a grain are
 * intentionally unsupported. Window/morph selection remains latched at spawn. */
static uint8_t gr_test_fidelity[NPART]; /* 0 legacy, 1 adaptive, 2 per-sample */
static uint8_t gr_test_fidelity_cap[NPART] = {4,4,4,4};
/* Per-sample prototype: decoder loops intentionally match gr_run verbatim;
 * only envelope evaluation and phase/countdown placement differ. */
static int gr_run_sample(gr_part_t *P, gr_grain_t *g, int32_t *acc, uint32_t n)
{
    uint32_t i, m = n < g->left ? n : g->left, step = g->step, pos = g->pos;
    int32_t a = g->a, b = g->b, frac = g->frac;
    const smp_zone_t *z = g->z;
    uint32_t phase = g->wph;
    if (!g->rev) {
        int32_t pred = g->pred, idx = g->idx;
        for (i = 0; i < m; i++) {
            int32_t s = a + (((b - a) * (frac >> 1)) >> 15);
            int32_t gain = mulq15(GR_RENDER_WIN(g, phase), g->gain);
            acc[i] += (s * gain) >> 15;
            phase += g->winc;
            frac += (int32_t)step;
            while (frac >= 65536) {
                frac -= 65536;
                a = b;
                b = pos < z->n ? gr_dec(z, pos++, &pred, &idx) : a;
            }
        }
        g->pred = pred;
        g->idx = idx;
    } else {
        int16_t *rb = P->rb[g - P->g];
        for (i = 0; i < m; i++) {
            int32_t s = a + (((b - a) * (frac >> 1)) >> 15);
            int32_t gain = mulq15(GR_RENDER_WIN(g, phase), g->gain);
            acc[i] += (s * gain) >> 15;
            phase += g->winc;
            frac -= (int32_t)step;
            while (frac < 0) {
                frac += 65536;
                b = a;
                if (pos) {                          /* (fitted: always) */
                    pos--;
                    if (pos < g->rlo)
                        gr_fill(P, g, rb, g->zl, pos);
                }
                a = rb[pos - g->rlo];
            }
        }
    }
    g->wph = phase;
    g->left -= m;
    g->pos = pos;
    g->a = a;
    g->b = b;
    g->frac = frac;
    if (!g->left)
        g->owner = 0;
    return g->left != 0;
}


/* Cap 4/8/16; invalid controls fail back to exact legacy rendering. Duration
 * bands use phase increment comparisons, avoiding a variable duration divide.
 * Remaining-life halving ensures the interior is sampled even for N=2..4. */
static int gr_run_fidelity(gr_part_t *P, gr_grain_t *g, int32_t *acc, uint32_t n, uint32_t method, uint32_t cap)
{
    if (!n || !g->left) return gr_run(P,g,acc,n);
    if (method == 2u) return gr_run_sample(P,g,acc,n);
    if (method != 1u || (cap != 4u && cap != 8u && cap != 16u)) return gr_run(P,g,acc,n);
    uint32_t base_step=g->winc>=UINT32_MAX/16u?1u:g->winc>=UINT32_MAX/32u?2u:g->winc>=UINT32_MAX/64u?4u:g->winc>=UINT32_MAX/256u?8u:16u;
    if(base_step>cap)base_step=cap;
    uint32_t done=0;
    while (done<n && g->left) {
        uint32_t step=base_step;
        if (g->left<=2u*step) {uint32_t half=g->left/2u;if(!half)half=1;if(step>half)step=half;}
        if (step>n-done)step=n-done;
        gr_run(P,g,acc+done,step);done+=step;
    }
    return g->left != 0;
}
