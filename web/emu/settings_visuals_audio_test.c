/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Huegelton Instruments */
/* Render helper follows native_check.c (Leo Kuroshita, Huegelton Instruments).
 * Identical real DSP gestures with old/new SETTINGS visuals; compare output bytes. */
#ifndef SETTINGS_WEB_SOURCE
#define SETTINGS_WEB_SOURCE "felucca_web.c"
#endif
#include SETTINGS_WEB_SOURCE
#include "../../tests/audio_stats.h"
static audio_stats_t sounding;
static FILE *out;
static int rec;
static void render(double ms)                     /* as emu_test.mjs: 128-frame calls, the last one shorter */
{
    uint32_t n = (uint32_t)(ms * 44.1 + 0.5), k, i;
    for (k = 0; k < n; k += 128u) {
        uint32_t m = n - k < 128u ? n - k : 128u;
        web_render(m);
        for (i = 0; rec && i < m; i++) {
            float s[2] = {out_l[i], out_r[i]};
            fwrite(s, sizeof s, 1, out);
            audio_stats_push(&sounding,s[0]);audio_stats_push(&sounding,s[1]);
        }
    }
}

int main(int argc,char **argv)
{
    if(argc!=2 || !(out=fopen(argv[1],"wb")))return 2;
    web_master(700);web_nor_erase();web_boot();render(4000);web_test_heavy();
    web_buttons(1u<<B_PLAY);render(60);web_buttons(0);render(60);rec=1;
    for(unsigned tab=0;tab<MTAB_COUNT;tab++) {
        ui.menu=1;ui.menu_sel=menu_visible_row(tab,0);ui.force=1;
        render(100);
        if(tab==MTAB_AUDIO)web_enc(EN_K1,1); /* actual SPEAKER EQ edit */
        render(100);
        if(tab==MTAB_AUDIO){ui.menu_sel=MI_TUNE;web_enc(EN_K1,1);}
        render(200);
    }
    fclose(out);
    printf("final browser audio: peak=%.6f RMS=%.6f nonzero=%llu/%llu\n",sounding.peak,audio_stats_rms(&sounding),
           (unsigned long long)sounding.nonzero,(unsigned long long)sounding.samples);
    return !audio_stats_sounding(&sounding,0.001,0.0001);
}
