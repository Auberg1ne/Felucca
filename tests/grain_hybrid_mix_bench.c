/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "audio_stats.h"
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
static uint64_t ticks(void){struct timespec t;clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t);return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
static int compare_u64(const void *a,const void *b){uint64_t x=*(const uint64_t*)a,y=*(const uint64_t*)b;return (x>y)-(x<y);}
int main(int argc,char **argv){
 FILE *f=argc>1?fopen(argv[1],"w"):stdout;if(!f)return 2;
 fprintf(f,"personality,fast,method,cap,threshold,optimize,max_grains,mean_grains,ns_per_frame,p99_block_ns,max_block_ns,peak,RMS,grain_hash\n");
 const unsigned methods[]={0,1,1,2,3,3,3,3,3,3},caps[]={8,4,8,8,4,8,4,8,4,8},thresholds[]={32,32,32,32,32,32,64,64,128,128};
 for(unsigned type=1;type<=3;type+=2)for(unsigned fast=0;fast<2;fast++)for(unsigned p=0;p<10;p++)for(unsigned opt=0;opt<(methods[p]==3?2u:1u);opt++){
  /* Fresh process per policy: ui_power_on is a UI fixture, not a full mixer/DSP reset. */
  fflush(f);pid_t child=fork();if(child<0)return 2;if(child){int status;if(waitpid(child,&status,0)<0 || !WIFEXITED(status) || WEXITSTATUS(status))return 1;continue;}
  ui_power_on();memset(gr_rng,0,sizeof gr_rng);song.master_q12=4096;
  for(unsigned c=0;c<4;c++){
   gr_test_fidelity[c]=methods[p];gr_test_fidelity_cap[c]=caps[p];gr_test_hybrid_cap[c]=caps[p];gr_test_hybrid_threshold[c]=thresholds[p];gr_test_hybrid_opt[c]=opt;gr_test_morph_mode[c]=1;gr_test_morph[c]=96;
   track_t *t=&trk[c];host_preset(t,ENGI_GRAIN,0);t->p[P_LEVEL]=100;t->p[P_MUTE]=0;t->p[P_ATK]=t->p[P_DEC]=t->p[P_REL]=0;t->p[P_SUS]=127;
   t->p[P_DIST]=t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=t->p[P_ITYPE]=0;
   t->p[P_ED_FLT]=t->p[P_LD_FLT]=t->p[P_ED_PIT]=t->p[P_LD_PIT]=t->p[P_ED_SHP]=t->p[P_LD_SHP]=t->p[P_LD_AMP]=0;t->p[P_PAN]=t->p[P_SPRD]=0;
   const int e[]={13,64,127,127,fast?24:-12,0,fast?127:0,127};for(unsigned k=0;k<8;k++)t->p[P_E0+k]=e[k];t->p[P_GTYPE]=type;t->p[P_GA]=t->p[P_GB]=127;t->p[P_GCUT]=t->p[P_GRES]=t->p[P_GFILTER_RSVD1]=0;
   trk_note_on(t,fast?72:48,100);
  }
  for(unsigned b=0;b<2048;b++){int32_t out[CTL*2];mix_block(out,CTL);}
  uint64_t times[2048],sum=0,grain_sum=0;unsigned grain_max=0;audio_stats_t stats={0};
  for(unsigned b=0;b<2048;b++){
   int32_t out[CTL*2];uint64_t start=ticks();mix_block(out,CTL);times[b]=ticks()-start;sum+=times[b];unsigned grains=0;
   for(unsigned c=0;c<4;c++)for(unsigned j=0;j<GR_NG;j++)grains+=gr_of(&trk[c])->g[j].owner!=0;
   if(grains>grain_max)grain_max=grains;grain_sum+=grains;for(unsigned j=0;j<CTL*2;j++)audio_stats_push(&stats,out[j]);
  }
  uint64_t hash=1469598103934665603ull;
  for(unsigned c=0;c<4;c++){
   const uint8_t *bytes=(const uint8_t*)gr_of(&trk[c])->head;
   for(unsigned i=0;i<sizeof gr_of(&trk[c])->head;i++)hash=(hash^bytes[i])*1099511628211ull;
   for(unsigned j=0;j<GR_NG;j++)if(gr_of(&trk[c])->g[j].owner){bytes=(const uint8_t*)&gr_of(&trk[c])->g[j];for(unsigned i=0;i<sizeof(gr_grain_t);i++)hash=(hash^bytes[i])*1099511628211ull;}
   hash=(hash^(uint32_t)gr_rng[c])*1099511628211ull;
  }
  if(!audio_stats_sounding(&stats,2,.1))return 1;qsort(times,2048,sizeof *times,compare_u64);
  fprintf(f,"%u,%u,%u,%u,%u,%u,%u,%.4f,%.6f,%llu,%llu,%.0f,%.6f,%llu\n",type,fast,methods[p],caps[p],thresholds[p],opt,grain_max,grain_sum/2048.,(double)sum/(2048*CTL),(unsigned long long)times[2027],(unsigned long long)times[2047],stats.peak,audio_stats_rms(&stats),(unsigned long long)hash);
  fflush(f);_exit(0);
 }
 if(f!=stdout)fclose(f);return 0;
}
