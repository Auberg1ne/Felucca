/* SPDX-License-Identifier: GPL-3.0-only */
/* Real browser backend, runnable natively; no mocked audio or replacement WAV. */
#include "felucca_web.c"
#include <math.h>
static unsigned checks;
#define REQUIRE(x) do {checks++;if(!(x)){fprintf(stderr,"SYNTH line %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    web_nor_erase();web_boot();
    /* Public browser render entrypoint: real MIDI, envelope, mixer and float output. */
    for(unsigned i=0;i<200;i++)web_render(128); /* finish splash */
    host_preset(&trk[0],ENGI_GRAIN,0);trk[0].p[P_E0]=13;trk[0].p[P_GCUT]=100;
    web_midi(0x90,60,127);
    double web_energy=0;unsigned web_nonzero=0;double web_peak=0;
    for(unsigned j=0;j<1400;j++) {
        web_render(128);
        for(unsigned i=0;i<128;i++) {
            double a=out_l[i];web_energy+=a*a;if(fabs(a)>0.00001)web_nonzero++;
            if(fabs(a)>web_peak)web_peak=fabs(a);
        }
    }
    printf("SYNTH browser MIDI/GRAIN: nonzero=%u peak=%.6f RMS=%.6f\n",web_nonzero,web_peak,sqrt(web_energy/(1400*128)));
    REQUIRE(web_nonzero>10000 && web_peak>0.001 && web_energy>0.01);
    web_midi(0x80,60,0);
    const smp_zone_t *z=gr_zone(13,0);
    REQUIRE(smp_factory_from_source(13)==5 && smp_user_from_source(13)==8);
    REQUIRE(!smp_set_missing(13) && gr_find(13,60)==0);
    REQUIRE(z->off==187658 && z->n==275098 && z->rate==65536 && z->root16==960 && !z->looped);
    REQUIRE(z->off+(z->n+1u)/2u<=sizeof SMP_DATA);
    vmod_t m={0};m.pitch16=60*16;m.amp0=m.amp1=32767;
    for(unsigned engine=0;engine<2;engine++) {
        track_t *t=&trk[0];host_preset(t,engine?ENGI_GRAIN:ENGI_SAMPLE,0);t->p[P_E0]=13;
        /* Explicit audible filter: isolate sample availability from cutoff settings. */
        t->p[P_GCUT]=100;
        voice_t *v=&t->v[0];memset(v,0,sizeof *v);v->note=60;v->active=1;
        if(engine){grain_note_on(t,v);for(unsigned i=0;i<1100;i++)grain_block(t);}
        else sample_note_on(t,v);
        double energy=0;unsigned nonzero=0,peak=0;
        for(unsigned j=0;j<44100/CTL;j++) {
            int32_t out[CTL]={0};
            if(engine){grain_block(t);grain_render(t,v,out,CTL,&m);}else sample_render(t,v,out,CTL,&m);
            for(unsigned i=0;i<CTL;i++){unsigned a=out[i]<0?-(int64_t)out[i]:out[i];if(a)nonzero++;if(a>peak)peak=a;energy+=(double)out[i]*out[i];}
        }
        double rms=sqrt(energy/((44100/CTL)*CTL));
        printf("SYNTH %s: nonzero=%u peak=%u RMS=%.2f, index entries=%u, state=%zu/%u bytes\n",engine?"GRAIN":"SAMPLE",nonzero,peak,rms,engine?gr_of(t)->zcnt[0]:0,sizeof(gr_part_t),ENG_MEM_PART);
        REQUIRE(nonzero>22050 && peak>1000 && rms>100);
        if(engine) {
            gr_part_t *P=gr_of(t);REQUIRE(P->zcnt[0]==1075 && P->zdone[0]==1075);
            /* Seek near the end beyond 16-bit sample/byte boundaries. */
            int32_t pred=0,idx=0,a,b;for(unsigned p=0;p<z->n-1;p++)gr_dec(z,p,&pred,&idx);
            gr_seek(P,z,0,z->n-1,&a,&b);REQUIRE(a==pred && b==idx);
        }
    }
    REQUIRE(sizeof(gr_part_t)<=ENG_MEM_PART && sizeof eng_mem==NPART*ENG_MEM_PART);
    printf("SYNTH audio: %u checks passed\n",checks);return 0;
}
