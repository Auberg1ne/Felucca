/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FELUCCA_GRAIN_STEREO_PROTOTYPE
#define FELUCCA_GRAIN_STEREO_PROTOTYPE 1
#endif
#ifndef FELUCCA_GRAIN_STEREO_INTEGRATION
#define FELUCCA_GRAIN_STEREO_INTEGRATION 1
#endif
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <time.h>
#include <pthread.h>
static unsigned bench_stage,bench_tracks,bench_voices;
static gr_part_t kernel_part[4];static gr_lr_filter_t filter_state[8];static voice_t mono_state[8];
static uint8_t worker_stack[262144] __attribute__((aligned(4096)));
static uint64_t bench_hash;static double bench_ns;static unsigned bench_max[4],bench_bad;
static void *worker(void *unused){
 const unsigned blocks=8192;vmod_t m={0};m.amp0=m.amp1=32767;clock_t start=clock();uint64_t hash=0;
 for(unsigned b=0;b<blocks;b++){
  if(bench_stage>=10){
   memset(mix_l,0,sizeof mix_l);memset(mix_r,0,sizeof mix_r);memset(send_c,0,sizeof send_c);memset(send_d,0,sizeof send_d);memset(send_r,0,sizeof send_r);
   for(unsigned k=0;k<bench_tracks;k++){
    track_t*t=&trk[k];int32_t mid[CTL];T_SPREAD(t)=1;
    for(unsigned i=0;i<CTL;i++){mid[i]=(i&1)?6000:-6000;SPREAD_BUF(t)[i]=mid[i]/2;gr_of(t)->stereo_side[0][i]=mid[i]/8;gr_of(t)->stereo_side[1][i]=-mid[i]/12;}
    if(bench_stage==10)mix_spread(t,mid,CTL SM_ARG(&mx[0]));else mix_grain_stereo(t,mid,CTL SM_ARG(&mx[0]));
   }
   for(unsigned i=0;i<CTL;i++)hash=hash*33+(uint32_t)mix_l[i]+(uint32_t)mix_r[i];
  }
  else if(bench_stage<=1||bench_stage>=8){int32_t out[CTL*2];mix_block(out,CTL);for(unsigned i=0;i<CTL*2;i++)hash=hash*33+(uint32_t)out[i];
   if(!(b&63u))for(unsigned k=0;k<bench_tracks;k++){
    unsigned count=0,load=0;gr_part_t*P=gr_of(&trk[k]);for(unsigned j=0;j<GR_NG;j++)if(P->g[j].owner){count++;load+=P->g[j].step<<P->g[j].rev;}
    if(count>bench_max[k])bench_max[k]=count;bench_bad|=count>GR_NG||load>GR_LOAD_MAX;
   }
  }
  else if(bench_stage<=5)for(unsigned v=0;v<bench_voices;v++){
   int32_t in[3][CTL],out[3][CTL]={{0}};
   for(unsigned ch=0;ch<3;ch++)for(unsigned i=0;i<CTL;i++)in[ch][i]=((b*CTL+i+ch*13)%64<32)?60000:-60000;
   if(bench_stage==3){gr_tone(&mono_state[v],in[0],CTL,gr_tone_coef(64));gr_filter(&mono_state[v],in[0],out[0],CTL,&m,25,127);}
   else gr_lr_filter(&filter_state[v],in,out,CTL,&m,gr_tone_coef(64),25,127,bench_stage==4?2:3);
   for(unsigned i=0;i<CTL;i++)hash=hash*33+(uint32_t)out[0][i];
  }
  else for(unsigned k=0;k<bench_tracks;k++){
   int32_t mono[CTL]={0};gr_part_t*P=&kernel_part[k];memset(P->lr_work,0,sizeof P->lr_work);
   for(unsigned j=0;j<GR_NG;j++){if(bench_stage==6)gr_run(P,&P->g[j],mono,CTL);else gr_run_lr(P,&P->g[j],mono,CTL);}
   for(unsigned i=0;i<CTL;i++)hash=hash*33+(uint32_t)mono[i]+(uint32_t)P->lr_work[0][i]+(uint32_t)P->lr_work[1][i];
  }
 }
 bench_ns=1e9*(clock()-start)/CLOCKS_PER_SEC/(blocks*CTL);bench_hash=hash;return 0;
}
int main(int argc,char**argv){
 if(argc!=3)return 2;bench_stage=atoi(argv[1]);bench_tracks=atoi(argv[2]);if(bench_tracks!=1&&bench_tracks!=4)return 2;
 ui_power_on();song.master_q12=4096;bench_voices=bench_tracks==1?3:8;
 for(unsigned k=0;k<4;k++){
  track_t*t=&trk[k];host_preset(t,ENGI_GRAIN,0);t->p[P_GTYPE]=0;t->p[P_VOICE]=V_POLY;t->p[P_E0]=13;t->p[P_E2]=80;t->p[P_E3]=127;t->p[P_E4]=0;t->p[P_E6]=127;t->p[P_E7]=64;t->p[P_GCUT]=25;t->p[P_GRES]=127;
  t->p[P_ATK]=t->p[P_DEC]=t->p[P_REL]=0;t->p[P_SUS]=127;t->p[P_LEVEL]=100;t->p[P_DIST]=t->p[P_ITYPE]=t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=0;
  if(bench_stage==8||bench_stage==9){t->p[P_DIST]=40;t->p[P_ITYPE]=1;t->p[P_IMIX]=80;t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=60;t->p[P_SPRD]=127;t->p[P_SLCR]=1;t->p[P_SLDEPTH]=100;fx_lowcut=2;}
  gr_test_spread[k]=(bench_stage==1||bench_stage==9)?100:0;
  gr_reset(gr_of(t),13,gr_stamp(13));for(unsigned i=0;i<GR_NIDX;i++)gr_build(gr_of(t),t);gr_rng[k]=0x12345678+k;
  if(k<bench_tracks)for(unsigned note=0;note<(bench_tracks==1?3:2);note++)trk_note_on(t,24+k*2+note*7,100);
  for(unsigned j=0;j<GR_NG;j++){gr_grain_t*g=&kernel_part[k].g[j];g->z=gr_zone(13,0);g->owner=1;g->a=g->b=16000;g->gain=9000;g->left=UINT32_MAX;g->winc=32768;g->window=192;kernel_part[k].pan[j]=(j&1)?48:-48;}
 }
 for(unsigned v=0;v<8;v++){filter_state[v].tone_a=mono_state[v].s[6]=GR_TONE_UNITY;filter_state[v].filter=mono_state[v].s[7]=256u|(75u<<9);}
 memset(worker_stack,0xa5,sizeof worker_stack);pthread_attr_t attr;pthread_attr_init(&attr);if(pthread_attr_setstack(&attr,worker_stack,sizeof worker_stack))return 3;
 pthread_t th;if(pthread_create(&th,&attr,worker,0))return 4;pthread_join(th,0);pthread_attr_destroy(&attr);
 size_t untouched=0;while(untouched<sizeof worker_stack&&worker_stack[untouched]==0xa5)untouched++;
 printf("%.3f %llu %zu %u %u %u %u\n",bench_ns,(unsigned long long)bench_hash,sizeof worker_stack-untouched,bench_max[0],bench_max[1],bench_max[2],bench_max[3]);return bench_bad?5:0;
}
