/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#ifndef CUTOFF_UI_SOURCE
#define CUTOFF_UI_SOURCE "ui_test.c"
#endif
#include CUTOFF_UI_SOURCE
#include "audio_stats.h"
static unsigned checks;
#define REQUIRE(x) do { checks++; if (!(x)) { fprintf(stderr,"cutoff zero line %u: %s\n",__LINE__,#x); return 1; } } while (0)
int main(void)
{
    /* Silence, an empty capture, and non-finite audio cannot validate a comparison. */
    audio_stats_t stats = {0};
    REQUIRE(!audio_stats_sounding(&stats,32,1));
    for (unsigned i=0;i<100;i++) audio_stats_push(&stats,0);
    REQUIRE(!audio_stats_sounding(&stats,32,1));
    audio_stats_push(&stats,NAN);
    REQUIRE(!audio_stats_sounding(&stats,32,1));
    /* The existing bipolar AMP modulation can boost a full envelope above unity. */
    const int ramps[][2] = {{0,0},{32767,32767},{0,28000},{28000,0},
                            {65527,65527},{0,65527},{65527,0}};
    const unsigned sizes[] = {0,1,17,CTL,CTL+8};
    for (unsigned type=0;type<4;type++) for (unsigned r=0;r<NELEM(ramps);r++)
        for (unsigned z=0;z<NELEM(sizes);z++) {
            ui_power_on();track_t *t=&trk[0];
            t->p[P_E0]=5;usr_nz[0]=0;t->p[P_GTYPE]=(int16_t)type;t->p[P_GCUT]=0;t->p[P_GRES]=127;t->p[P_E4]=0;
            grain_block(t);voice_t *v=&t->v[0];v->note=60;v->active=1;grain_note_on(t,v);
            REQUIRE(v->s[0]==GR_SINE); /* analytic dry input, independent of the filter implementation */
            v->ph[0]=123456789u;v->s[2]=123;v->s[3]=-456;v->s[4]=789;
            vmod_t m={0};m.pitch16=960;m.amp0=ramps[r][0];m.amp1=ramps[r][1];
            int32_t out[CTL+12],expected[CTL+12];
            for (unsigned i=0;i<NELEM(out);i++) out[i]=expected[i]=1000+(int32_t)i*11;
            unsigned n=sizes[z]<CTL?sizes[z]:CTL;uint32_t ph=v->ph[0],inc=pitch_inc(960);
            for (unsigned i=0;i<n;i++) {
                int32_t sample=sine_i(ph)>>1;
                int32_t amp=m.amp0+(m.amp1-m.amp0)*(int32_t)i/(int32_t)CTL;
                int64_t gained=((int64_t)sample*amp)>>15;
                expected[i]+=(int32_t)(((gained*VOICE_FS)>>15)*2);
                ph+=inc;
            }
            grain_render(t,v,out,sizes[z],&m);
            REQUIRE(!memcmp(out,expected,sizeof out)); /* accumulation, both ramps, silence and untouched canaries */
            REQUIRE(v->ph[0]==ph);
            REQUIRE(v->s[2]==0 && v->s[3]==0 && v->s[4]==0);
        }
    /* Stress the accumulator limit with a bounded synthetic sustained grain pool.
     * No source decoding is needed inside this single block; it exercises the real grain path. */
    for (unsigned type=0;type<4;type++)for (unsigned sign=0;sign<2;sign++)for (unsigned r=0;r<NELEM(ramps);r++) {
        ui_power_on();track_t *t=&trk[0];t->p[P_E0]=3;t->p[P_GTYPE]=(int16_t)type;t->p[P_GB]=127;t->p[P_GCUT]=0;
        gr_part_t *p=gr_of(t);memset(p,0,sizeof *p);gr_reset(p,3,0);
        voice_t *v=&t->v[0];memset(v,0,sizeof *v);v->active=1;v->s[0]=0;v->s[1]=10000;
        for (unsigned g=0;g<GR_NG;g++) {
            p->g[g].z=gr_zone(3,0);p->g[g].owner=1;p->g[g].left=CTL*2;
            p->g[g].a=p->g[g].b=sign?-30000:30000;p->g[g].step=256;
            p->g[g].wph=128u<<24;p->g[g].gain=32767;
        }
        vmod_t m={0};m.pitch16=960;m.amp0=ramps[r][0];m.amp1=ramps[r][1];
        int32_t out[CTL];for (unsigned i=0;i<CTL;i++)out[i]=789;
        grain_render(t,v,out,CTL,&m);
        for (unsigned i=0;i<CTL;i++) {
            int32_t amp=m.amp0+(m.amp1-m.amp0)*(int32_t)i/(int32_t)CTL;
            int64_t x=((int64_t)(sign?-65535:65535)*amp)>>15;
            REQUIRE(out[i]==789+(int32_t)(((x*VOICE_FS)>>15)*2));
        }
    }
    printf("GRAIN cutoff zero: %u checks passed; dry accumulation, analytic envelope ramps, filter reset and bounds\n",checks);
    return 0;
}
