/* SPDX-License-Identifier: GPL-3.0-only */
/* Host capacity proxy, not target timing. A remains the only production kernel. */
#define UI_TEST_NO_MAIN 1
#ifndef WIN_I2_UI_SOURCE
#define WIN_I2_UI_SOURCE "ui_test.c"
#endif
#include WIN_I2_UI_SOURCE
#include <time.h>
static volatile int64_t observed;
static uint64_t ns(void){struct timespec t;clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t);return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
int main(void){
 printf("sizes,grain=%zu,part=%zu,track=%zu,engine=%u\n",sizeof(gr_grain_t),sizeof(gr_part_t),sizeof(track_t),ENG_MEM_PART);
 puts("WIN,reverse,rate,grains,repeat,ns_per_frame,max_block_ns,endpoint_evals,table_interpolations");
 const unsigned wins[]={0,64,96,127};
 for(unsigned w=0;w<4;w++)for(unsigned rev=0;rev<2;rev++)for(unsigned fast=0;fast<2;fast++)for(unsigned repeat=0;repeat<5;repeat++){
  gr_part_t parts[4]={0};track_t t={0};const smp_zone_t*z=gr_zone(13,0);unsigned per=fast?(rev?2:4):12;
  for(unsigned c=0;c<4;c++){
   gr_reset(&parts[c],13,0);for(unsigned k=0;k<GR_NIDX;k++)gr_build(&parts[c],&t);
   for(unsigned j=0;j<per;j++){
    gr_grain_t*g=&parts[c].g[j];g->owner=1;g->z=z;g->step=fast?131072:16384;g->gain=GR_NORM[per];g->left=86432;g->winc=UINT32_MAX/g->left;g->rev=rev;
#ifdef WIN_I2_PRODUCTION
    g->window=128+wins[w];
#endif
    if(rev){g->pos=z->n-2;gr_fill(&parts[c],g,parts[c].rb[j],0,g->pos+1);g->a=parts[c].rb[j][g->pos-g->rlo];g->b=parts[c].rb[j][g->pos+1-g->rlo];}
    else{g->a=gr_dec(z,0,&g->pred,&g->idx);g->b=gr_dec(z,1,&g->pred,&g->idx);g->pos=2;}
   }
  }
  uint64_t total=0,max=0;for(unsigned b=0;b<1024;b++){
   uint64_t start=ns();for(unsigned c=0;c<4;c++){int32_t out[CTL]={0};for(unsigned j=0;j<per;j++)gr_run(&parts[c],&parts[c].g[j],out,CTL);for(unsigned k=0;k<CTL;k++)observed+=out[k];}
   uint64_t dt=ns()-start;total+=dt;if(dt>max)max=dt;
  }
  unsigned evaluations=4*per*2,lookups=evaluations;
#ifdef WIN_I2_PRODUCTION
  if(wins[w]!=0 && wins[w]!=64 && wins[w]!=127)lookups*=2;
#endif
  printf("%u,%u,%u,%u,%u,%.6f,%llu,%u,%u\n",wins[w],rev,fast?131072:16384,4*per,repeat,total/(1024.*CTL),(unsigned long long)max,evaluations,lookups);
 }
 return observed==INT64_MIN;
}
