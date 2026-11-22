/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"WIN I2 latch %u: %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned occupied(const gr_part_t*p){unsigned n=0;for(unsigned i=0;i<GR_NG;i++)n+=p->g[i].owner!=0;return n;}
int main(void){
 _Static_assert(sizeof(gr_grain_t)==64 && sizeof(gr_part_t)==7576 && sizeof(track_t)==1840,"host state growth");
 CHECK(offsetof(gr_grain_t,window)==offsetof(gr_grain_t,zl)+1);
 CHECK(offsetof(gr_part_t,win_snapshot)==offsetof(gr_part_t,head_zl)+GR_POLY);
 ui_power_on();vmod_t m={0};m.pitch16=960;m.shape=64<<8;
 for(unsigned part=0;part<4;part++){
  track_t*t=&trk[part];host_preset(t,ENGI_GRAIN,0);t->p[P_E0]=0;t->p[P_E1]=64;t->p[P_E2]=80;t->p[P_E6]=127;t->v[0].active=1;
  gr_part_t*p=gr_of(t);memset(p,0,sizeof *p);gr_reset(p,0,0);for(unsigned k=0;k<GR_NIDX;k++)gr_build(p,t);
  for(unsigned type=0;type<4;type++)for(unsigned val=0;val<128;val++){
   memset(p->g,0,sizeof p->g);t->p[P_GTYPE]=type;t->p[P_GA]=0;t->p[P_GB]=127;t->p[109]=val;
   grain_block(t);CHECK(p->win_snapshot==val);
   /* A later raw control write does not change this block's spawn snapshot. */
   t->p[109]=(val+37)%128;gr_spawn(p,t,0,0,500,&m);unsigned count=0;
   for(unsigned j=0;j<GR_NG;j++)if(p->g[j].owner){CHECK(p->g[j].window==128+val);count++;}
   CHECK(count==(type==3?2u:1u));gr_part_t frozen=*p;
   grain_block(t);CHECK(p->win_snapshot==(val+37)%128);
   unsigned blocks=0;
   while(occupied(p)){
    for(unsigned j=0;j<GR_NG;j++)if(p->g[j].owner){
     struct{uint32_t pre;int32_t a[CTL];uint32_t post;}a={.pre=123,.post=456},b=a;
     gr_run(p,&p->g[j],a.a,CTL);gr_run(&frozen,&frozen.g[j],b.a,CTL);
     CHECK(!memcmp(a.a,b.a,sizeof a.a) && a.pre==123 && a.post==456 && b.pre==123 && b.post==456);
     CHECK(!memcmp(&p->g[j],&frozen.g[j],sizeof p->g[j]));
     CHECK(!memcmp(p->rb[j],frozen.rb[j],sizeof p->rb[j]));
     CHECK(p->g[j].window==128+val);
    }
    CHECK(++blocks<10000);
   }
   memset(p->g,0,sizeof p->g);gr_spawn_one(p,t,0,0,500,&m,0,32767);CHECK(p->g[0].owner && p->g[0].window==128+(val+37)%128);
  }
  /* Invalid int16 control range, not merely invalid unsigned bytes. */
  for(int val=INT16_MIN;val<=INT16_MAX;val++){t->p[109]=val;grain_block(t);CHECK(p->win_snapshot==(val>=0 && val<=127?val:64));}
  t->eng_req=ENGI_FM6;t->p[109]=0;grain_block(t);CHECK(p->win_snapshot==64);t->eng_req=ENGI_GRAIN;
  for(unsigned bad=128;bad<256;bad++){memset(p->g,0,sizeof p->g);p->win_snapshot=bad;gr_spawn_one(p,t,0,0,500,&m,0,32767);CHECK(p->g[0].owner && p->g[0].window==192);}
 }
 printf("WIN I2 latching: %u checks passed; block snapshot, every value/personality/track, SHIMMER pairs, active-grain PCM/state, invalid controls\n",checks);return 0;
}
