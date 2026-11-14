/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Huegelton Instruments */
/* Render helper follows native_check.c (Leo Kuroshita, Huegelton Instruments).
 * Identical real DSP gestures with old/new MIXER visuals; compare output bytes. */
#include "felucca_web.c"
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
        }
    }
}

int main(int argc,char **argv)
{
    if(argc!=2 || !(out=fopen(argv[1],"wb")))return 2;
    web_nor_erase();web_boot();render(4000);web_test_heavy();home_tap();ui_draw();
#ifdef CLOCK_AUDIO
    home_tap();ui_draw();
    if(ui.home || cur_page()->scope!=SC_GLOBAL)return 3;
#else
    if(ui.home || cur_page()->scope!=SC_TRK)return 3;
#endif
    web_buttons(1u<<B_PLAY);render(60);web_buttons(0);render(60);rec=1;
    render(500);web_enc(EN_ALGO,1);render(100);
    web_enc(EN_K2,-1);render(400);web_enc(EN_K3,1);render(250);
    web_enc(EN_K4,1);render(250);web_enc(EN_K4,-1);render(500);
    fclose(out);return 0;
}
