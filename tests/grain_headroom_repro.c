/* SPDX-License-Identifier: GPL-3.0-only */
/* Same bounded, synthetic three-grain drive on the exact base and repaired path. */
#define UI_TEST_NO_MAIN 1
#ifndef FILTER_UI_SOURCE
#define FILTER_UI_SOURCE "ui_test.c"
#endif
#include FILTER_UI_SOURCE
#include "audio_stats.h"
int main(void)
{
    ui_power_on();track_t *t=&trk[0];host_preset(t,ENGI_GRAIN,0);
    t->p[P_E0]=3;t->p[P_E7]=127;t->p[P_GCUT]=25;t->p[P_GRES]=127;
    gr_part_t *p=gr_of(t);memset(p,0,sizeof *p);gr_reset(p,3,gr_stamp(3));
    voice_t *v=&t->v[0];memset(v,0,sizeof *v);v->note=60;v->active=1;grain_note_on(t,v);
    vmod_t m={0};m.pitch16=960;m.shape=64<<8;m.amp0=m.amp1=32767;
    audio_stats_t stats={0};int peak=0;
    for(unsigned i=0;i<44100;i++) {
        int32_t sample=(int32_t)(32767*sin(2*3.141592653589793*sqrt(30.0*16000)*i/44100));
        for(unsigned j=0;j<3;j++) {
            gr_grain_t *g=&p->g[j];memset(g,0,sizeof *g);g->owner=1;g->z=gr_zone(3,0);
            g->left=2;g->a=g->b=sample;g->step=256;g->wph=128u<<24;g->gain=GR_NORM[3];
        }
        v->s[1]=100000;int32_t out=0;grain_render(t,v,&out,1,&m);audio_stats_push(&stats,out);
        if(abs(v->s[2])>peak)peak=abs(v->s[2]);
    }
    if(!audio_stats_sounding(&stats,100,10))return 1;
    printf("GRAIN bounded resonant drive: filter peak=%d output peak=%.0f RMS=%.6f nonzero=%llu\n",peak,stats.peak,audio_stats_rms(&stats),(unsigned long long)stats.nonzero);
    return 0;
}
