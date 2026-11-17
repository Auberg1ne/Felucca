/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared harness for exact-base/fix comparisons. Each process starts from clean state. */
#define UI_TEST_NO_MAIN 1
#ifndef CUTOFF_UI_SOURCE
#define CUTOFF_UI_SOURCE "ui_test.c"
#endif
#include CUTOFF_UI_SOURCE
#include "audio_stats.h"
#ifndef AUDIO_TEST_MASTER
#define AUDIO_TEST_MASTER 4096
#endif
int main(int argc,char **argv)
{
    if (argc<4) return 2;
    unsigned type=(unsigned)atoi(argv[1]),tracks=argc>4?(unsigned)atoi(argv[4]):1u;
    int switching=!strcmp(argv[2],"switch");
    int modtest=!strcmp(argv[2],"modtest");
    int cut=atoi(argv[2]);
    if (type>3 || !tracks || tracks>4 || (!switching && !modtest && (cut< -50 || cut>50))) return 2;
    FILE *audio=fopen(argv[3],"wb");if (!audio) return 2;
    ui_power_on();song.master_q12=AUDIO_TEST_MASTER;
    for (unsigned p=0;p<4;p++) {
        track_t *t=&trk[p];host_preset(t,ENGI_GRAIN,0);
        t->p[P_MUTE]=p>=tracks;t->p[P_LEVEL]=100;
        t->p[P_ATK]=t->p[P_DEC]=t->p[P_REL]=0;t->p[P_SUS]=127;
        t->p[P_DIST]=t->p[P_ITYPE]=t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=0;
        t->p[P_ED_FLT]=t->p[P_LD_FLT]=t->p[P_ED_SHP]=t->p[P_LD_SHP]=0;
        t->p[P_PAN]=t->p[P_SPRD]=0;
        const int e[]={3,64,80,90,0,40,30,127};
        for (unsigned i=0;i<8;i++)t->p[P_E0+i]=(int16_t)e[i];
        t->p[P_GTYPE]=(int16_t)type;t->p[P_GA]=t->p[P_GB]=64;t->p[108]=0;t->p[P_GRES]=64;
        if (p<tracks)trk_note_on(t,60+2*p,100);
    }
    /* Modulation test: same patch, same deterministic source;
     * only the existing LFO filter destination changes. */
    if (modtest) {
        /* Exercise the real voice-render LFO destination.
         * Set a slow rate and nonzero filter depth. */
        trk[0].p[P_GCUT]=0;
        trk[0].p[P_LD_FLT]=63;
        trk[0].p[P_LRATE]=0;
        trk[0].p[P_LFADE]=0;
    }
    const int transitions[]={-25,0,25,0,-25};
    unsigned stages=switching?NELEM(transitions):1u;
    for (unsigned stage=0;stage<stages;stage++) {
        audio_stats_t stats={0};
        for (unsigned p=0;p<tracks;p++)trk[p].p[P_GCUT]=(int16_t)(switching?transitions[stage]:(modtest?0:cut));
        for (unsigned b=0;b<(switching?1024u:4096u);b++) {
            int32_t out[CTL*2];mix_block(out,CTL);
            if (fwrite(out,sizeof out,1,audio)!=1) return 2;
            for (unsigned i=0;i<CTL*2;i++)audio_stats_push(&stats,out[i]);
            if (b==0 && trk[0].p[P_GCUT]==0)
                for (unsigned p=0;p<tracks;p++)for (unsigned v=0;v<GR_POLY;v++)if (trk[p].v[v].active)
                    if (trk[p].v[v].s[2] || trk[p].v[v].s[3] || trk[p].v[v].s[4]) return 1;
        }
        printf("type=%u tracks=%u cut=%d peak=%.0f RMS=%.6f nonzero=%llu/%llu\n",type,tracks,trk[0].p[P_GCUT],stats.peak,audio_stats_rms(&stats),
               (unsigned long long)stats.nonzero,(unsigned long long)stats.samples);
        if (!audio_stats_sounding(&stats,2,0.1)) return 1;
        if (tracks==4)for (unsigned p=0;p<4;p++)if (!home_level_read(p)) return 1;
    }
    return fclose(audio)!=0;
}
