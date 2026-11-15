/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef STRETCH_WEB_SOURCE
#define STRETCH_WEB_SOURCE "felucca_web.c"
#endif
#include STRETCH_WEB_SOURCE
/* Read either the exact old source or the split head when comparing revisions. */
static uint64_t head_q16(const gr_part_t *p,unsigned i)
{
#ifdef GR_HEAD_FRAC_BITS
 return ((uint64_t)p->head[i].sample<<16)|p->head[i].frac;
#else
 return p->head[i];
#endif
}
static FILE *out;static unsigned recording,frames;static double energy,peak;
static void render(unsigned ms)
{
 unsigned total=(unsigned)(ms*44.1+0.5);
 for(unsigned k=0;k<total;k+=128){unsigned n=total-k<128?total-k:128;web_render(n);
  if(recording)for(unsigned i=0;i<n;i++){float s[2]={out_l[i],out_r[i]};fwrite(s,sizeof s,1,out);frames++;
   for(unsigned c=0;c<2;c++){double v=s[c];energy+=v*v;if(fabs(v)>peak)peak=fabs(v);}}}
}
int main(int argc,char **argv)
{
 if(argc!=3 || !(out=fopen(argv[2],"wb")))return 2;
 unsigned stretch=(unsigned)atoi(argv[1]);if(stretch>127)return 2;
 web_nor_erase();web_boot();render(4000);
 for(unsigned i=0;i<4;i++)trk[i].p[P_MUTE]=1;
 host_preset(&trk[0],ENGI_GRAIN,0);track_t *t=&trk[0];
 t->p[P_MUTE]=0;t->p[P_E0]=3;t->p[P_E1]=0;t->p[P_E2]=80;t->p[P_E3]=90;t->p[P_E4]=0;t->p[P_E5]=0;t->p[P_E6]=0;
 t->p[P_GTYPE]=0;t->p[P_GB]=0;t->p[P_GCUT]=50;t->p[P_GRES]=0;t->p[P_GFILTER_RSVD1]=stretch;
 t->p[P_ATK]=0;t->p[P_DEC]=0;t->p[P_SUS]=127;t->p[P_REL]=0;t->p[P_REV]=t->p[P_DLY]=t->p[P_CHOR]=t->p[P_DIST]=0;
 open_family(FAM_EDIT); /* Exercise the live EDIT renderer during the audio regression. */
 render(1200);web_midi(0x90,60,127);render(50);
 uint64_t before=0,after=0;
 for(unsigned i=0;i<GR_POLY;i++)before+=head_q16(gr_of(t),i);
 recording=1;render(16);
 for(unsigned i=0;i<GR_POLY;i++)after+=head_q16(gr_of(t),i);
 render(484);fclose(out);
 double rms=sqrt(energy/(2*frames));if(frames!=22050 || peak<0.001 || rms<0.0001 || !isfinite(rms))return 1;
 uint64_t head=after-before;
 printf("STRETCH=%u frames=%u peak=%.6f RMS=%.6f head-Q16=%llu\n",stretch,frames,peak,rms,(unsigned long long)head);return 0;
}
