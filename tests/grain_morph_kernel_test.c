/* SPDX-License-Identifier: GPL-3.0-only */
/* Strict sanitizer execution of the actual production gr_run; no UI/mixer init. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks, short_silent;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"kernel line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
 _Static_assert(sizeof(gr_part_t)<=ENG_MEM_PART,"window pool exceeds engine reservation");
 _Static_assert(sizeof(gr_grain_t)==64 || sizeof(gr_grain_t)==56,"unexpected grain layout");
 const uint32_t lengths[]={16,17,31,32,33,441,86432};
 const uint32_t steps[]={256,32768,65536,131072};
 const int levels[]={-32768,-1,0,1,32767};
 for(unsigned w=0;w<128;w++)for(unsigned rev=0;rev<2;rev++)for(unsigned li=0;li<NELEM(lengths);li++)for(unsigned si=0;si<NELEM(steps);si++)for(unsigned vi=0;vi<NELEM(levels);vi++){
  gr_part_t p={0};smp_zone_t z=SMP_ZONES[0];gr_grain_t *g=&p.g[0];
  g->z=&z;g->owner=1;g->window=128u+w;g->rev=rev;g->left=lengths[li];g->winc=UINT32_MAX/g->left;g->gain=32767;g->step=steps[si];g->a=g->b=levels[vi];
  /* Reverse begins in a valid cached interval; forward decodes bounded built-in data. */
  g->pos=rev?100:2;g->rlo=0;for(unsigned k=0;k<GR_RB;k++)p.rb[0][k]=levels[vi];
  struct{unsigned pre;int32_t out[CTL];unsigned post;} acc={.pre=0x12345678,.post=0xabcdef};
  unsigned n=lengths[li]<CTL?lengths[li]:CTL;uint32_t before=g->left;
  gr_run(&p,g,acc.out,n);CHECK(acc.pre==0x12345678 && acc.post==0xabcdef);CHECK(g->left==before-n);CHECK(g->window==128u+w);
  for(unsigned i=0;i<CTL;i++)CHECK(acc.out[i]>=-32768 && acc.out[i]<=32767);
  if(!g->left)CHECK(!g->owner);
  if(lengths[li]<=CTL){for(unsigned i=0;i<CTL;i++)CHECK(acc.out[i]==0);short_silent++;}
 }
 /* Cross multiple real ADPCM reverse-cache refill boundaries under strict
  * instrumentation. Build the real source index without unrelated UI init. */
 for(unsigned w=0;w<128;w++){
  struct{uint32_t pre;gr_part_t p;uint32_t post;} box={.pre=0x12345678,.post=0xabcdef};
  gr_part_t *p=&box.p;track_t t={0};gr_reset(p,0,0);for(unsigned i=0;i<GR_NIDX;i++)gr_build(p,&t);
  gr_grain_t *g=&p->g[0];g->z=gr_zone(0,0);g->owner=1;g->window=128u+w;g->rev=1;g->pos=1024;
  g->left=400;g->winc=UINT32_MAX/g->left;g->gain=32767;g->step=GR_STEP_MAX;g->zl=0;
  gr_fill(p,g,p->rb[0],0,g->pos+1);g->a=p->rb[0][g->pos-g->rlo];g->b=p->rb[0][g->pos+1-g->rlo];
  unsigned refills=0;long long energy=0;
  while(g->owner){
   struct{uint32_t pre;int32_t v[CTL];uint32_t post;} out={.pre=0x1234,.post=0x5678};uint32_t old=g->rlo;
   gr_run(p,g,out.v,CTL);refills+=g->rlo!=old;
   CHECK(out.pre==0x1234 && out.post==0x5678 && box.pre==0x12345678 && box.post==0xabcdef);
   for(unsigned i=0;i<CTL;i++)energy+=llabs(out.v[i]);
  }
  CHECK(refills>=5 && energy>0);
 }
 /* Full long-grain lifetime at the rate cap in both directions, every morph.
  * SYNTH's 275098-frame zone safely contains this 172864-source-frame span. */
 {
  gr_part_t p={0};track_t t={0};gr_reset(&p,13,0);for(unsigned i=0;i<GR_NIDX;i++)gr_build(&p,&t);
  const smp_zone_t *z=gr_zone(13,0);CHECK(z->n>2u*86432u+3u);
  for(unsigned w=0;w<128;w++)for(unsigned rev=0;rev<2;rev++){
   memset(p.g,0,sizeof p.g);gr_grain_t *g=&p.g[0];g->owner=1;g->z=z;g->window=128+w;g->rev=rev;
   g->left=86432;g->winc=UINT32_MAX/g->left;g->gain=32767;g->step=GR_STEP_MAX;
   if(rev){g->pos=z->n-2;gr_fill(&p,g,p.rb[0],0,g->pos+1);g->a=p.rb[0][g->pos-g->rlo];g->b=p.rb[0][g->pos+1-g->rlo];}
   else{g->a=gr_dec(z,0,&g->pred,&g->idx);g->b=gr_dec(z,1,&g->pred,&g->idx);g->pos=2;}
   unsigned blocks=0;uint64_t energy=0;
   while(g->owner){
    struct{uint32_t pre;int32_t v[CTL];uint32_t post;} out={.pre=0x1234,.post=0x5678};
    gr_run(&p,g,out.v,CTL);CHECK(out.pre==0x1234 && out.post==0x5678 && g->window==128+w);CHECK(++blocks<=2701);
    for(unsigned i=0;i<CTL;i++)energy+=llabs(out.v[i]);
   }
   CHECK(energy>0);
  }
 }
 gr_grain_t invalid={0};invalid.window=127;
 const uint32_t phases[]={0,1,511,512,0x7fffffffu,0x80000000u,UINT32_MAX};
 for(unsigned i=0;i<NELEM(phases);i++)CHECK(gr_experiment_win(&invalid,phases[i])==gr_win(phases[i]));
 printf("morph kernel: %u checks passed; actual gr_run strict sanitizers, 16..86432 frames, rates, signed samples, directions, bounds; %u single-block cases retain existing silence\n",checks,short_silent);return 0;
}
