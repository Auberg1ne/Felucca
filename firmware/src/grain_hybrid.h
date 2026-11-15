/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Optimized adaptive wrapper derived from grain_fidelity.h. */
#pragma once
/* Test-only controls: configure before playback. No grain/track fields added. */
static uint8_t gr_test_hybrid_threshold[NPART]={32,32,32,32};
static uint8_t gr_test_hybrid_cap[NPART]={8,8,8,8};
static uint8_t gr_test_hybrid_opt[NPART]; /* 0 reference B, 1 exact-output optimization */
/* These exact thresholds use immutable phase increment, never remaining life.
 * floor(UINT32_MAX/N) >= floor(UINT32_MAX/T) iff N<=T for the tested T/range. */
static int gr_hybrid_sample(const gr_grain_t *g,uint32_t threshold)
{
    uint32_t limit=threshold==32u?UINT32_MAX/32u:threshold==64u?UINT32_MAX/64u:UINT32_MAX/128u;
    return g->winc>=limit;
}
/* Separate optimization: a one-frame legacy ramp outputs only w0. C(1)
 * produces exactly that result/state without unused w1/dq calculation. */
static int gr_run_adaptive_fast(gr_part_t *P,gr_grain_t *g,int32_t *acc,uint32_t n,uint32_t cap)
{
    if(!n || !g->left)return gr_run(P,g,acc,n);
    if(cap!=4u && cap!=8u)return gr_run(P,g,acc,n);
    uint32_t base=g->winc>=UINT32_MAX/16u?1u:g->winc>=UINT32_MAX/32u?2u:g->winc>=UINT32_MAX/64u?4u:g->winc>=UINT32_MAX/256u?8u:16u;
    if(base>cap)base=cap;
    uint32_t done=0;
    while(done<n && g->left){
        uint32_t step=base;
        if(g->left<=2u*step){uint32_t half=g->left/2u;if(!half)half=1;if(step>half)step=half;}
        if(step>n-done)step=n-done;
        if(step==1u)gr_run_sample(P,g,acc+done,1u);else gr_run(P,g,acc+done,step);
        done+=step;
    }
    return g->left!=0;
}
static int gr_run_envelope(gr_part_t *P,gr_grain_t *g,int32_t *acc,uint32_t n,uint32_t method,uint32_t cap,uint32_t threshold,uint32_t optimize)
{
    if(method!=3u)return gr_run_fidelity(P,g,acc,n,method,cap);
    if((threshold!=32u && threshold!=64u && threshold!=128u) || (cap!=4u && cap!=8u) || optimize>1u)return gr_run(P,g,acc,n);
    if(gr_hybrid_sample(g,threshold))return gr_run_sample(P,g,acc,n);
    return optimize?gr_run_adaptive_fast(P,g,acc,n,cap):gr_run_fidelity(P,g,acc,n,1u,cap);
}
