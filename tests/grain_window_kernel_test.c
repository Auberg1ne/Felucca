/* SPDX-License-Identifier: GPL-3.0-only */
/* Strict sanitizer execution of the actual production gr_run; no UI/mixer init. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks, short_silent;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"kernel line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
 const uint32_t lengths[]={16,17,31,32,33,441,86432};
 const uint32_t steps[]={256,32768,65536,131072};
 const int levels[]={-32768,-1,0,1,32767};
 for(unsigned w=0;w<2;w++)for(unsigned rev=0;rev<2;rev++)for(unsigned li=0;li<NELEM(lengths);li++)for(unsigned si=0;si<NELEM(steps);si++)for(unsigned vi=0;vi<NELEM(levels);vi++){
  gr_part_t p={0};smp_zone_t z=SMP_ZONES[0];gr_grain_t *g=&p.g[0];
  g->z=&z;g->owner=1;g->window=w;g->rev=rev;g->left=lengths[li];g->winc=UINT32_MAX/g->left;g->gain=32767;g->step=steps[si];g->a=g->b=levels[vi];
  /* Reverse begins in a valid cached interval; forward decodes bounded built-in data. */
  g->pos=rev?100:2;g->rlo=0;for(unsigned k=0;k<GR_RB;k++)p.rb[0][k]=levels[vi];
  struct{unsigned pre;int32_t out[CTL];unsigned post;} acc={.pre=0x12345678,.post=0xabcdef};
  unsigned n=lengths[li]<CTL?lengths[li]:CTL;uint32_t before=g->left;
  gr_run(&p,g,acc.out,n);CHECK(acc.pre==0x12345678 && acc.post==0xabcdef);CHECK(g->left==before-n);CHECK(g->window==w);
  for(unsigned i=0;i<CTL;i++)CHECK(acc.out[i]>=-32768 && acc.out[i]<=32767);
  if(!g->left)CHECK(!g->owner);
  if(lengths[li]<=CTL){for(unsigned i=0;i<CTL;i++)CHECK(acc.out[i]==0);short_silent++;}
 }
 printf("window kernel: %u checks passed; actual gr_run strict sanitizers, 16..86432 frames, rates, signed samples, directions, bounds; %u single-block cases retain existing silence\n",checks,short_silent);return 0;
}
