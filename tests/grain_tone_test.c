/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "audio_stats.h"
static unsigned checks;
#define REQUIRE(x) do { checks++; if (!(x)) { fprintf(stderr,"TONE line %u: %s\n",__LINE__,#x);return 1;} } while (0)
int main(void)
{
    REQUIRE(P_E7==117 && P_COUNT==118);
    REQUIRE(ENG_GRAIN.edit[7].min==0 && ENG_GRAIN.edit[7].max==127 && ENG_GRAIN.edit[7].def==127);
    int32_t last=0;
    for (int tone=0;tone<128;tone++) { int a=gr_tone_coef(tone);REQUIRE(a>last && a<=16384);last=a; }
    /* Exact bypass, accumulation-buffer bounds, zero-length calls and fresh-state ownership. */
    voice_t v={0};v.s[6]=16384;
    int32_t a[CTL+2],copy[CTL+2];
    for(unsigned i=0;i<NELEM(a);i++)a[i]=copy[i]=i&1?200000:-200000;
    gr_tone(&v,a+1,CTL,16384);REQUIRE(!memcmp(a,copy,sizeof a));REQUIRE(v.s[5]==clamp(copy[CTL],-65535,65535));
    voice_t old=v;gr_tone(&v,a,0,gr_tone_coef(0));REQUIRE(!memcmp(&old,&v,sizeof v));
    /* A full-scale step during live edits cannot overflow or overshoot the input hull.
     * Also compare sample-by-sample vs block partitioning: time is in samples. */
    voice_t block={0},single={0};block.s[6]=single.s[6]=16384;
    for(unsigned b=0;b<512;b++) {
        int target=gr_tone_coef((int[]){0,127,1,126,32,96,64}[b%7]);
        int32_t x[CTL+2],y[CTL];x[0]=1234567;x[CTL+1]=-1234567;
        for(unsigned i=0;i<CTL;i++)x[i+1]=y[i]=(b&1)?65535:-65535;
        gr_tone(&block,x+1,CTL,target);
        for(unsigned i=0;i<CTL;i++)gr_tone(&single,y+i,1,target);
        REQUIRE(x[0]==1234567 && x[CTL+1]==-1234567);
        REQUIRE(!memcmp(x+1,y,sizeof y) && block.s[5]==single.s[5] && block.s[6]==single.s[6]);
        REQUIRE(abs(block.s[5])<=65535 && block.s[6]>=gr_tone_coef(0) && block.s[6]<=16384);
    }
    /* A parameter jump changes the next output by at most one smoothed-coefficient
     * step relative to continuing at the old coefficient. Constant input stays flat. */
    for(unsigned tone=0;tone<128;tone++)for(unsigned next=0;next<128;next++) {
        voice_t w={0},frozen={0};w.s[5]=frozen.s[5]=-65535;w.s[6]=frozen.s[6]=gr_tone_coef(tone);
        int32_t changed=65535,unchanged=65535;
        gr_tone(&w,&changed,1,gr_tone_coef(next));gr_tone(&frozen,&unchanged,1,frozen.s[6]);
        REQUIRE(abs(changed-unchanged)<=512);
        w.s[5]=12345;int32_t dc=12345;gr_tone(&w,&dc,1,gr_tone_coef(next));REQUIRE(dc==12345);
    }
    /* Settles exactly into bypass, not an endless near-unity filter. */
    for(unsigned b=0;b<128;b++){int32_t x[CTL]={0};gr_tone(&block,x,CTL,16384);}
    REQUIRE(block.s[6]==16384);
    /* Steady sine RMS response verifies monotonic frequency-dependent attenuation,
     * independently of envelopes, random grain starts and PCM hash differences. */
    const int tones[]={0,1,32,64,96,126,127},freqs[]={100,500,2000,8000,14000,20000};
    for(unsigned f=0;f<NELEM(freqs);f++) {
        double previous=0;
        for(unsigned t=0;t<NELEM(tones);t++) {
            voice_t w={0};w.s[6]=gr_tone_coef(tones[t]);audio_stats_t stats={0};
            for(unsigned b=0;b<512;b++) {
                int32_t x[CTL];for(unsigned i=0;i<CTL;i++)x[i]=(int32_t)(30000*sin(2*3.141592653589793*freqs[f]*(b*CTL+i)/44100));
                gr_tone(&w,x,CTL,w.s[6]);
                for(unsigned i=0;i<CTL;i++){REQUIRE(abs(w.s[5])<=30000);if(b>32)audio_stats_push(&stats,x[i]);}
            }
            double rms=audio_stats_rms(&stats);REQUIRE(audio_stats_sounding(&stats,1,1));REQUIRE(rms>previous);previous=rms;
            if(freqs[f]>=8000 && tones[t]==0)REQUIRE(rms<300); /* >37 dB attenuation */
            printf("tone=%d frequency=%d RMS=%.6f\n",tones[t],freqs[f],rms);
        }
    }
    ui_power_on();track_t *t=&trk[0];voice_t *w=&t->v[0];t->p[P_E7]=0;w->env=0;w->s[5]=999999;w->s[6]=-123;
    grain_note_on(t,w);REQUIRE(w->s[5]==0 && w->s[6]==gr_tone_coef(0));
    w->env=1;w->s[5]=123;w->s[6]=456;grain_note_on(t,w);REQUIRE(w->s[5]==123 && w->s[6]==456);
    printf("GRAIN TONE: %u checks passed; spectral monotonicity, strict numerical bounds, bypass and state lifetime\n",checks);
    return 0;
}
