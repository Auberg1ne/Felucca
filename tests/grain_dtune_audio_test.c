/* SPDX-License-Identifier: GPL-3.0-only */
/* Identical controls against the exact base/fix. Synthetic user data is test-only. */
#define UI_TEST_NO_MAIN 1
#ifndef DTUNE_UI_SOURCE
#define DTUNE_UI_SOURCE "ui_test.c"
#endif
#include DTUNE_UI_SOURCE
#include "audio_stats.h"
static uint64_t position_q16(const gr_part_t *p,unsigned vi)
{
#ifdef GR_HEAD_FRAC_BITS
    return ((uint64_t)p->head[vi].sample<<16)|p->head[vi].frac;
#else
    return p->head[vi];
#endif
}
int main(int argc,char **argv)
{
    if (argc!=12) return 2;
    unsigned user=!strcmp(argv[2],"short") || !strcmp(argv[2],"medium") || !strcmp(argv[2],"fractional");
    unsigned type=(unsigned)atoi(argv[1]),src=user?5u:(unsigned)atoi(argv[2]);
    unsigned detune=(unsigned)atoi(argv[3]),modev=(unsigned)atoi(argv[4]),spread=(unsigned)atoi(argv[8]),tracks=(unsigned)atoi(argv[10]); int ptch=atoi(argv[5]); unsigned rand=(unsigned)atoi(argv[6]),extreme=(unsigned)atoi(argv[7]);
    const char *mode=argv[9];
    if (type>3 || src>14 || detune>127 || modev>V_UNISON || spread>127 || rand>127 || extreme>1 || ptch< -24 || ptch>24 || !tracks || tracks>4) return 2;
    FILE *audio=fopen(argv[11],"wb");if (!audio)return 2;
    ui_power_on();song.master_q12=4096;if(!strcmp(mode,"residual"))song.g[G_TUNE]=1;
    if (src==5) {
        usr_nz[0]=1;
        unsigned len=!strcmp(argv[2],"medium")?20003u:4099u;
        usr_zone[0][0]=(smp_zone_t){.off=16,.n=len,.ls=0,.le=len-1,
            .rate=!strcmp(argv[2],"fractional")?65792u:65536u,.root16=960,.lo=0,.hi=127};
    }
    for (unsigned p=0;p<4;p++) {
        track_t *t=&trk[p];host_preset(t,ENGI_GRAIN,0);
        t->p[P_MUTE]=p>=tracks;t->p[P_LEVEL]=100;t->p[P_ATK]=t->p[P_DEC]=t->p[P_REL]=0;t->p[P_SUS]=127;
        t->p[P_DIST]=t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=t->p[P_ITYPE]=0;
        t->p[P_ED_FLT]=t->p[P_LD_FLT]=t->p[P_ED_SHP]=t->p[P_LD_SHP]=t->p[P_LD_AMP]=0;
        t->p[P_PAN]=0;t->p[P_SPRD]=spread;t->p[P_VOICE]=modev;t->p[P_DETUNE]=detune;
        const int e[]={src,64,80,90,ptch,40,rand,127};for(unsigned i=0;i<8;i++)t->p[P_E0+i]=(int16_t)e[i];
        t->p[P_GTYPE]=type;t->p[P_GA]=type==0?(extreme?127:0):(type==3?(extreme?127:0):64);t->p[P_GB]=64;
        t->p[P_GCUT]=0;t->p[P_GRES]=0;t->p[P_GFILTER_RSVD1]=extreme?127:0;
        gr_part_t *g=gr_of(t);memset(g,0,sizeof *g);gr_reset(g,src,gr_stamp(src));
        for (unsigned i=0;i<GR_NIDX;i++)gr_build(g,t);
        if(!strcmp(mode,"residual"))midi_bend_q8[p]=midi_bend_target[p]=7;
        if(p<tracks)trk_note_on(t,!strcmp(mode,"capped")?60:48,100);
    }
    int32_t initial_fine[NPART][3];for(unsigned p=0;p<tracks;p++)for(unsigned vi=0;vi<3;vi++)initial_fine[p][vi]=trk[p].v[vi].fine;
    audio_stats_t stats={0};uint64_t head_hash=1469598103934665603ull,step_hash=1469598103934665603ull;
    unsigned born[3]={0},lo=UINT32_MAX,hi=0,maxvoices=0;double stereo_diff=0;
    for(unsigned b=0;b<2048;b++) {
        if(!strcmp(mode,"held") && b==512)for(unsigned p=0;p<tracks;p++)trk[p].p[P_DETUNE]=127;
        if(!strcmp(mode,"retrigger") && b==512)for(unsigned p=0;p<tracks;p++){trk[p].p[P_DETUNE]=127;trk_note_on(&trk[p],50,100);}
        int32_t out[CTL*2];mix_block(out,CTL);if(fwrite(out,sizeof out,1,audio)!=1)return 2;
        for(unsigned i=0;i<CTL*2;i++)audio_stats_push(&stats,out[i]);
        for(unsigned i=0;i<CTL*2;i+=2){double d=(double)out[i]-out[i+1];stereo_diff+=d*d;}
        unsigned active=0;
        for(unsigned p=0;p<tracks;p++) {
            gr_part_t *g=gr_of(&trk[p]);
            for(unsigned vi=0;vi<3;vi++) {
                if(trk[p].v[vi].active && trk[p].v[vi].stage!=4)active++;
                if(!strcmp(mode,"held") && trk[p].v[vi].fine!=initial_fine[p][vi])return 1;
                head_hash=(head_hash^position_q16(g,vi))*1099511628211ull;
            }
            for(unsigned i=0;i<GR_NG;i++)if(g->g[i].owner) {
                if(g->g[i].pos>=g->g[i].z->n || g->g[i].step<256 || g->g[i].step>GR_STEP_MAX)return 1;
                step_hash=(step_hash^g->g[i].step)*1099511628211ull;
                if(g->g[i].step<lo)lo=g->g[i].step;if(g->g[i].step>hi)hi=g->g[i].step;
                born[g->g[i].owner-1]++;
            }
        }
        if(active>NVOICE)return 1;if(active>maxvoices)maxvoices=active;
    }
    if(!audio_stats_sounding(&stats,2,0.1))return 1;
    if(tracks==4)for(unsigned p=0;p<4;p++)if(!home_level_read(p))return 1;
    if(modev==V_UNISON && tracks==1 && !rand && ptch<=0)for(unsigned vi=0;vi<3;vi++)if(!born[vi])return 1;
    printf("type=%u src=%u DTUNE=%u VCE=%u PTCH=%d RAND=%u extreme=%u spread=%u mode=%s tracks=%u peak=%.0f RMS=%.6f nonzero=%llu/%llu fine=[%d,%d,%d] steps=[%u,%u] head-hash=%llu step-hash=%llu maxvoices=%u stereo-energy=%.6f\n",
      type,src,detune,modev,ptch,rand,extreme,spread,mode,tracks,stats.peak,audio_stats_rms(&stats),(unsigned long long)stats.nonzero,(unsigned long long)stats.samples,
      trk[0].v[0].fine,trk[0].v[1].fine,trk[0].v[2].fine,lo,hi,(unsigned long long)head_hash,(unsigned long long)step_hash,maxvoices,stereo_diff);
    return fclose(audio)!=0;
}
