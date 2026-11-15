/* SPDX-License-Identifier: GPL-3.0-only */
/* Identical controls against the exact base/fix. Synthetic user data is test-only. */
#define UI_TEST_NO_MAIN 1
#ifndef POSITION_UI_SOURCE
#define POSITION_UI_SOURCE "ui_test.c"
#endif
#include POSITION_UI_SOURCE
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
    if (argc!=8) return 2;
    unsigned user=!strcmp(argv[2],"short") || !strcmp(argv[2],"medium") || !strcmp(argv[2],"fractional");
    unsigned type=(unsigned)atoi(argv[1]),src=user?5u:(unsigned)atoi(argv[2]);
    unsigned pos=(unsigned)atoi(argv[3]),stretch=(unsigned)atoi(argv[4]),tracks=(unsigned)atoi(argv[6]);
    const char *mode=argv[5];
    if (type>3 || src>14 || pos>127 || stretch>127 || !tracks || tracks>4) return 2;
    FILE *audio=fopen(argv[7],"wb");if (!audio)return 2;
    ui_power_on();song.master_q12=4096;
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
        t->p[P_PAN]=t->p[P_SPRD]=0;
        const int e[]={src,pos,80,90,0,40,30,127};for(unsigned i=0;i<8;i++)t->p[P_E0+i]=(int16_t)e[i];
        if (!strcmp(mode,"control"))t->p[P_E6]=0; /* no reverse fractional-boundary correction to compare */
        t->p[P_GTYPE]=type;t->p[P_GA]=t->p[P_GB]=64;t->p[P_GCUT]=0;t->p[P_GRES]=0;t->p[P_GFILTER_RSVD1]=stretch;
        if (!strcmp(mode,"shape")) {t->p[P_LD_SHP]=60;t->p[P_LRATE]=100;t->p[P_LFADE]=0;}
        if (!strcmp(mode,"freeze")) {t->p[P_E2]=t->p[P_GA]=127;t->p[P_E4]=24;t->p[P_E6]=127;}
        gr_part_t *g=gr_of(t);memset(g,0,sizeof *g);gr_reset(g,src,gr_stamp(src));
        for (unsigned i=0;i<GR_NIDX;i++)gr_build(g,t);
        if(p<tracks)trk_note_on(t,60+2*p,100);
    }
    unsigned age=trk[0].v[0].age;audio_stats_t stats={0};
    uint64_t first=0,last=0;unsigned changes=0,wraps=0;uint32_t minhead=UINT32_MAX,maxhead=0;
    unsigned blocks=!strcmp(mode,"traverse")?20000u:4096u;
    for (unsigned b=0;b<blocks;b++) {
        if (!strcmp(mode,"pos") && (b==1024 || b==2048))for(unsigned p=0;p<tracks;p++)trk[p].p[P_E1]=b==1024?127:0;
        if (!strcmp(mode,"source") && b==1024) {
            unsigned next=src==0?2:src==2?3:src==3?13:src==13?14:0;
            for(unsigned p=0;p<tracks;p++)trk[p].p[P_E0]=(int16_t)next;
        }
        int32_t out[CTL*2];mix_block(out,CTL);if(fwrite(out,sizeof out,1,audio)!=1)return 2;
        for(unsigned i=0;i<CTL*2;i++)audio_stats_push(&stats,out[i]);
        uint64_t h=position_q16(gr_of(&trk[0]),0);
        if(!b)first=h;if(h!=last)changes++;if(b && h<last)wraps++;last=h;
        uint32_t sample=(uint32_t)(h>>16);if(sample<minhead)minhead=sample;if(sample>maxhead)maxhead=sample;
        if(trk[0].v[0].age!=age || !trk[0].v[0].active)return 1; /* no forced retrigger */
    }
    if(!audio_stats_sounding(&stats,2,0.1))return 1;
    if(tracks==4)for(unsigned p=0;p<4;p++)if(!home_level_read(p))return 1;
    printf("type=%u src=%u pos=%u stretch=%u mode=%s tracks=%u peak=%.0f RMS=%.6f nonzero=%llu/%llu head-first=%llu head-last=%llu head-min=%u head-max=%u head-changes=%u wraps=%u zone-frames=%u\n",
           type,src,pos,stretch,mode,tracks,stats.peak,audio_stats_rms(&stats),(unsigned long long)stats.nonzero,
           (unsigned long long)stats.samples,(unsigned long long)first,(unsigned long long)last,minhead,maxhead,changes,wraps,gr_zone(src,0)->n);
    return fclose(audio)!=0;
}
