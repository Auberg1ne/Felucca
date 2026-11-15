/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <time.h>
static uint64_t ticks(void){struct timespec t;clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t);return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
static volatile int64_t checksum;
int main(int argc,char **argv){
 FILE *f=argc>1?fopen(argv[1],"w"):stdout;if(!f)return 2;
 fprintf(f,"reverse,window,rate,method,cap,threshold,optimize,grains,blocks,ns_per_frame,max_block_ns,window_evaluations_per_block\n");
 const unsigned methods[]={0,1,1,2,3,3,3,3,3,3},caps[]={8,4,8,8,4,8,4,8,4,8},thresholds[]={32,32,32,32,32,32,64,64,128,128};
 for(unsigned rev=0;rev<2;rev++)for(unsigned win=0;win<4;win++)for(unsigned rate=0;rate<2;rate++)for(unsigned p=0;p<10;p++)for(unsigned opt=0;opt<(methods[p]==3?2u:1u);opt++){
  gr_part_t parts[4]={0};track_t t={0};const smp_zone_t *z=gr_zone(13,0);unsigned perpart=rate?(rev?2:4):12; /* rate 2, decode budget 8 */
  for(unsigned c=0;c<4;c++){
   gr_reset(&parts[c],13,0);for(unsigned i=0;i<GR_NIDX;i++)gr_build(&parts[c],&t);
   for(unsigned j=0;j<perpart;j++){
    gr_grain_t *g=&parts[c].g[j];g->z=z;g->owner=1;g->window=(uint8_t[]){128,192,255,224}[win];g->rev=rev;g->gain=GR_NORM[perpart];g->step=rate?131072:16384;g->left=86432;g->winc=UINT32_MAX/g->left;
    if(rev){g->pos=z->n-2;gr_fill(&parts[c],g,parts[c].rb[j],0,g->pos+1);g->a=parts[c].rb[j][g->pos-g->rlo];g->b=parts[c].rb[j][g->pos+1-g->rlo];}
    else{g->a=gr_dec(z,0,&g->pred,&g->idx);g->b=gr_dec(z,1,&g->pred,&g->idx);g->pos=2;}
   }
  }
  const unsigned blocks=1024;uint64_t total=0,max=0;
  for(unsigned b=0;b<blocks;b++){
   uint64_t begin=ticks();
   for(unsigned c=0;c<4;c++){int32_t out[CTL]={0};for(unsigned j=0;j<perpart;j++)gr_run_envelope(&parts[c],&parts[c].g[j],out,CTL,methods[p],caps[p],thresholds[p],opt);for(unsigned i=0;i<CTL;i++)checksum+=out[i];}
   uint64_t duration=ticks()-begin;total+=duration;if(duration>max)max=duration;
  }
  unsigned eval=methods[p]==2?32:methods[p]==0?2:caps[p]==4?16:8; /* long, nonterminal blocks */
  fprintf(f,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%.6f,%llu,%u\n",rev,(unsigned[]){128,192,255,224}[win],rate?131072:16384,methods[p],caps[p],thresholds[p],opt,4*perpart,blocks,(double)total/(blocks*CTL),(unsigned long long)max,eval*4*perpart);
 }
 if(f!=stdout)fclose(f);return checksum==INT64_MIN;
}
