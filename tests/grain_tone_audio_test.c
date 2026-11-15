/* SPDX-License-Identifier: GPL-3.0-only */
/* Identical controls against the exact base/fix. Synthetic user data is test-only. */
#define UI_TEST_NO_MAIN 1
#ifndef TONE_UI_SOURCE
#define TONE_UI_SOURCE "ui_test.c"
#endif
#include TONE_UI_SOURCE
#include "audio_stats.h"
int main(int argc,char **argv)
{
    if (argc!=9) return 2;
    unsigned user=!strcmp(argv[2],"user") || !strcmp(argv[2],"sine");
    unsigned type=(unsigned)atoi(argv[1]),src=user?5u:(unsigned)atoi(argv[2]);
    unsigned tone=(unsigned)atoi(argv[3]),res=(unsigned)atoi(argv[5]),tracks=(unsigned)atoi(argv[7]); int cut=atoi(argv[4]);
    const char *mode=argv[6];
    if (type>3 || src>14 || tone>127 || res>127 || cut< -50 || cut>50 || !tracks || tracks>4) return 2;
    FILE *audio=fopen(argv[8],"wb");if (!audio)return 2;
    ui_power_on();song.master_q12=4096;
    if (src==5) {
        usr_nz[0]=1;
        unsigned len=20003u; if(!strcmp(argv[2],"sine"))usr_nz[0]=0;
        usr_zone[0][0]=(smp_zone_t){.off=16,.n=len,.ls=0,.le=len-1,
            .rate=65536u,.root16=960,.lo=0,.hi=127};
    }
    for (unsigned p=0;p<4;p++) {
        track_t *t=&trk[p];host_preset(t,ENGI_GRAIN,0);
        t->p[P_MUTE]=p>=tracks;t->p[P_LEVEL]=100;t->p[P_ATK]=t->p[P_DEC]=t->p[P_REL]=0;t->p[P_SUS]=127;
        t->p[P_DIST]=t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=t->p[P_ITYPE]=0;
        t->p[P_ED_FLT]=t->p[P_LD_FLT]=t->p[P_ED_SHP]=t->p[P_LD_SHP]=t->p[P_LD_AMP]=0;
        t->p[P_PAN]=t->p[P_SPRD]=0;
        const int e[]={src,64,80,90,0,40,30,tone};for(unsigned i=0;i<8;i++)t->p[P_E0+i]=(int16_t)e[i];
        t->p[P_GTYPE]=type;t->p[P_GA]=t->p[P_GB]=64;t->p[P_GCUT]=cut;t->p[P_GRES]=res;t->p[P_GFILTER_RSVD1]=0;
        gr_part_t *g=gr_of(t);memset(g,0,sizeof *g);gr_reset(g,src,gr_stamp(src));
        for (unsigned i=0;i<GR_NIDX;i++)gr_build(g,t);
        if(p<tracks)trk_note_on(t,60+2*p,100);
    }
    unsigned age=trk[0].v[0].age;audio_stats_t stats={0};
    double diff_energy=0; int32_t previous=0;
    for (unsigned b=0;b<2048;b++) {
        if (!strcmp(mode,"live") && b%256==0)for(unsigned p=0;p<tracks;p++)trk[p].p[P_E7]=(int[]){127,0,127,64,1,126,96,32}[b/256];
        int32_t out[CTL*2];mix_block(out,CTL);if(fwrite(out,sizeof out,1,audio)!=1)return 2;
        for(unsigned i=0;i<CTL*2;i++)audio_stats_push(&stats,out[i]);
        for(unsigned i=0;i<CTL*2;i+=2){double d=(double)out[i]-previous;diff_energy+=d*d;previous=out[i];}
#ifdef GR_TONE_UNITY
        for(unsigned p=0;p<tracks;p++)for(unsigned j=0;j<GR_POLY;j++)if(trk[p].v[j].active) {
            if(abs(trk[p].v[j].s[5])>65535 || trk[p].v[j].s[6]<gr_tone_coef(0) || trk[p].v[j].s[6]>GR_TONE_UNITY)return 1;
        }
#endif
        if(trk[0].v[0].age!=age || !trk[0].v[0].active)return 1; /* no forced retrigger */
    }
    if(!audio_stats_sounding(&stats,2,0.1))return 1;
    if(tracks==4)for(unsigned p=0;p<4;p++)if(!home_level_read(p))return 1;
    printf("type=%u src=%u tone=%u cut=%d res=%u mode=%s tracks=%u peak=%.0f RMS=%.6f nonzero=%llu/%llu HF=%.9f\n",
           type,src,tone,cut,res,mode,tracks,stats.peak,audio_stats_rms(&stats),(unsigned long long)stats.nonzero,
           (unsigned long long)stats.samples,diff_energy/(stats.energy/2));
    return fclose(audio)!=0;
}
