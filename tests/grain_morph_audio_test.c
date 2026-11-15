/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#ifndef MORPH_UI_SOURCE
#define MORPH_UI_SOURCE "ui_test.c"
#endif
#include MORPH_UI_SOURCE
#include "audio_stats.h"
int main(int argc,char **argv)
{
 if(argc!=7)return 2;
 const char *mode=argv[1];unsigned control=atoi(argv[2]),type=atoi(argv[3]),scenario=atoi(argv[5]);
 int off=!strcmp(mode,"off"),independent=!strcmp(mode,"independent"),live=!strcmp(mode,"live");
 if(!off && !independent && !live && strcmp(mode,"morph"))return 2;
 int user=!strcmp(argv[4],"user"),sine=!strcmp(argv[4],"sine");unsigned src=user||sine?5:atoi(argv[4]);
 if(control>127 || (off && control>2) || type>3 || scenario>2)return 2;
#if defined(FELUCCA_GRAIN_WINDOW_EXPERIMENT) && FELUCCA_GRAIN_WINDOW_EXPERIMENT
 gr_test_window=off?control:0;
#else
 if(!off || control)return 2;
#endif
#if defined(FELUCCA_GRAIN_MORPH_EXPERIMENT) && FELUCCA_GRAIN_MORPH_EXPERIMENT
 for(unsigned p=0;p<4;p++){gr_test_morph_mode[p]=!off;gr_test_morph[p]=independent?(control+p*37)%128:control;}
#else
 if(!off)return 2;
#endif
 FILE *f=fopen(argv[6],"wb");if(!f)return 2;
 ui_power_on();song.master_q12=4096;
 if(user){usr_nz[0]=1;usr_zone[0][0]=(smp_zone_t){.off=16,.n=20003,.ls=0,.le=20002,.rate=65536,.root16=960,.lo=0,.hi=127};}
 if(sine)usr_nz[0]=0;
 for(unsigned ch=0;ch<4;ch++){
  track_t *t=&trk[ch];host_preset(t,ENGI_GRAIN,0);
  t->p[P_LEVEL]=100;t->p[P_MUTE]=0;t->p[P_ATK]=t->p[P_DEC]=t->p[P_REL]=0;t->p[P_SUS]=127;
  t->p[P_DIST]=t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=t->p[P_ITYPE]=0;
  t->p[P_ED_FLT]=t->p[P_LD_FLT]=t->p[P_ED_SHP]=t->p[P_LD_SHP]=t->p[P_LD_AMP]=0;
  t->p[P_PAN]=t->p[P_SPRD]=0;
  const int e[]={src,64,scenario==0?0:scenario==1?80:127,127,0,40,127,127};
  for(unsigned k=0;k<8;k++)t->p[P_E0+k]=e[k];
  t->p[P_GTYPE]=type;t->p[P_GA]=scenario==0?0:127;t->p[P_GB]=127;
  t->p[P_GCUT]=t->p[P_GRES]=t->p[P_GFILTER_RSVD1]=0;
  gr_part_t *p=gr_of(t);memset(p,0,sizeof *p);gr_reset(p,src,gr_stamp(src));for(unsigned i=0;i<GR_NIDX;i++)gr_build(p,t);
  trk_note_on(t,60+ch,100);
 }
 audio_stats_t stats={0};double de=0;int32_t prev[2]={0},maxjump=0;unsigned fw=0,rv=0;
 for(unsigned b=0;b<4096;b++){
#if defined(FELUCCA_GRAIN_MORPH_EXPERIMENT) && FELUCCA_GRAIN_MORPH_EXPERIMENT
  if(live && !(b%256))for(unsigned p=0;p<4;p++)gr_test_morph[p]=(control+p*37+(b/256)*17)%128;
#endif
  struct{uint32_t pre;int32_t out[CTL*2];uint32_t post;} buf={.pre=0x12345678,.post=0xabcdef};
  int32_t *out=buf.out;mix_block(out,CTL);if(buf.pre!=0x12345678 || buf.post!=0xabcdef)return 1;
  if(fwrite(out,sizeof buf.out,1,f)!=1)return 2;
  for(unsigned i=0;i<CTL*2;i++){
   audio_stats_push(&stats,out[i]);int32_t d=out[i]-prev[i&1];prev[i&1]=out[i];de+=(double)d*d;if(abs(d)>maxjump)maxjump=abs(d);
  }
  for(unsigned ch=0;ch<4;ch++)for(unsigned i=0;i<GR_NG;i++)if(gr_of(&trk[ch])->g[i].owner){if(gr_of(&trk[ch])->g[i].rev)rv++;else fw++;
#if defined(FELUCCA_GRAIN_MORPH_EXPERIMENT) && FELUCCA_GRAIN_MORPH_EXPERIMENT
   if(!live && gr_of(&trk[ch])->g[i].window!=(off?control:128+(independent?(control+ch*37)%128:control)))return 1;
#endif
  }
 }
 if(!audio_stats_sounding(&stats,2,0.1))return 1;
 if(!sine && (!fw || !rv))return 1;
 for(unsigned ch=0;ch<4;ch++)if(!home_level_read(ch))return 1;
 printf("mode=%s control=%u type=%u source=%s scenario=%u peak=%.0f RMS=%.9f nonzero=%llu energy=%.0f diff-energy=%.0f max-jump=%d forward=%u reverse=%u grain-bytes=%zu part-bytes=%zu track-bytes=%zu\n",mode,control,type,argv[4],scenario,stats.peak,audio_stats_rms(&stats),(unsigned long long)stats.nonzero,stats.energy,de,maxjump,fw,rv,sizeof(gr_grain_t),sizeof(gr_part_t),sizeof(track_t));
 return fclose(f)!=0;
}
