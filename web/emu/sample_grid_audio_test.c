/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef SAMPLE_WEB_SOURCE
#define SAMPLE_WEB_SOURCE "felucca_web.c"
#endif
#include SAMPLE_WEB_SOURCE
static FILE*out;static unsigned recording,frames,expected;static double energy,peak,tail_energy;
static void render(unsigned ms){unsigned total=(unsigned)(ms*44.1+0.5);for(unsigned k=0;k<total;k+=128){unsigned n=total-k<128?total-k:128;web_render(n);
 if(recording)for(unsigned i=0;i<n;i++){float s[]={out_l[i],out_r[i]};fwrite(s,sizeof s,1,out);frames++;for(unsigned c=0;c<2;c++){energy+=s[c]*s[c];if(frames>expected-4410u)tail_energy+=s[c]*s[c];if(fabs(s[c])>peak)peak=fabs(s[c]);}}}}
int main(int argc,char**argv){
 if((argc!=5 && argc!=7) || !(out=fopen(argv[4],"wb")))return 2;unsigned engine=atoi(argv[1]),source=atoi(argv[2]),mode=atoi(argv[3]);
 unsigned ms=argc==7?(unsigned)atoi(argv[5]):500u,loop=argc==7?(unsigned)atoi(argv[6]):1u;
 if(ms<500 || ms>10000 || loop>1)return 2;expected=(unsigned)(ms*44.1+0.5);
 web_nor_erase();web_boot();render(4000);for(unsigned i=0;i<4;i++)trk[i].p[P_MUTE]=1;
 track_t*t=&trk[0];host_preset(t,engine,0);t->p[P_MUTE]=0;t->p[P_E0]=source;t->p[P_VOICE]=mode;
 t->p[P_GLIDE]=23;t->p[P_GLMODE]=1;t->p[P_PRIO]=1;t->p[P_ALLOC]=1;t->p[P_DETUNE]=63;t->p[P_SPRD]=72;t->p[P_PAN]=18;
 t->p[P_ATK]=0;t->p[P_DEC]=0;t->p[P_SUS]=127;t->p[P_REL]=20;t->p[P_REV]=t->p[P_DLY]=t->p[P_CHOR]=t->p[P_DIST]=0;
 if(engine==ENGI_SAMPLE){t->p[P_E1]=5;t->p[P_E2]=35;t->p[P_E3]=loop;t->p[P_E4]=100;t->p[P_E6]=40;}
 else{t->p[P_GCUT]=50;t->p[P_GRES]=0;t->p[P_GFILTER_RSVD1]=64;t->p[P_E1]=0;t->p[P_E2]=80;t->p[P_E3]=90;}
 open_family(FAM_EDIT);render(1200);web_midi(0x90,60,100);render(30);recording=1;render(ms);fclose(out);
 double rms=sqrt(energy/(2*frames));if(frames!=expected || peak<0.001 || rms<0.0001 || !isfinite(rms))return 1;
 double tail=sqrt(tail_energy/(2*4410));
 if(ms>=7000 && engine==ENGI_SAMPLE && ((source==0 && loop)?tail<0.0001:tail>0.000001))return 1;
 printf("engine=%u source=%u mode=%u loop=%u frames=%u peak=%.6f RMS=%.6f tail-RMS=%.6f\n",engine,source,mode,loop,frames,peak,rms,tail);return 0;
}
