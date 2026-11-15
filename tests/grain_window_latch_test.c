/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"latch line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
 ui_power_on();track_t *t=&trk[0];host_preset(t,ENGI_GRAIN,0);
 t->p[P_E0]=0;t->p[P_E7]=127;t->p[P_E6]=127;t->p[P_E1]=64;t->p[P_E5]=40;
 vmod_t m={0};m.pitch16=960;m.shape=64<<8;
 for(unsigned type=0;type<4;type++)for(unsigned size=0;size<2;size++)for(unsigned hold=0;hold<2;hold++)for(unsigned dir=0;dir<2;dir++)for(unsigned w=0;w<3;w++){
  t->p[P_GTYPE]=type;t->p[P_GA]=hold?127:0;t->p[P_GB]=127;t->p[P_E2]=size?127:0;
  gr_part_t *p=gr_of(t);memset(p,0,sizeof *p);gr_reset(p,0,0);for(unsigned i=0;i<GR_NIDX;i++)gr_build(p,t);
  gr_test_window=w;
  for(unsigned seed=1;seed<10000;seed++){
   p->g[0].owner=0;gr_rng[0]=seed;gr_spawn_one(p,t,0,0,500,&m,0,32767);
   if(p->g[0].owner && p->g[0].rev==dir)break;
  }
  CHECK(p->g[0].owner && p->g[0].rev==dir && p->g[0].window==w);
  gr_part_t frozen=*p;gr_test_window=(w+1)%3;
  unsigned blocks=0;long long energy=0;
  while(p->g[0].owner){
   struct {uint32_t pre;int32_t v[CTL];uint32_t post;} a={.pre=0x1234,.post=0x5678},b=a;
   gr_run(p,&p->g[0],a.v,CTL);gr_run(&frozen,&frozen.g[0],b.v,CTL);
   CHECK(!memcmp(a.v,b.v,sizeof a.v));CHECK(a.pre==0x1234 && a.post==0x5678 && b.pre==0x1234 && b.post==0x5678);
   CHECK(p->g[0].window==w);for(unsigned i=0;i<CTL;i++)energy+=llabs(a.v[i]);CHECK(++blocks<10000);
  }
  CHECK(energy>0);
  gr_spawn_one(p,t,0,0,500,&m,0,32767);CHECK(p->g[0].owner && p->g[0].window==(w+1)%3);
 }
 /* Invalid test selector cannot become a table index. */
 for(unsigned value=3;value<=255;value++){gr_test_window=value;gr_part_t *p=gr_of(t);memset(p->g,0,sizeof p->g);gr_spawn_one(p,t,0,0,500,&m,0,32767);CHECK(p->g[0].owner && p->g[0].window==0);}
 printf("window latching: %u checks passed; all personalities, sizes, HOLD, directions, canaries; grain=%zu part=%zu\n",checks,sizeof(gr_grain_t),sizeof(gr_part_t));return 0;
}
