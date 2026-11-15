/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"morph latch %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 ui_power_on();for(unsigned p=0;p<4;p++)CHECK(gr_test_morph_mode[p]==0 && gr_test_morph[p]==64);vmod_t m={0};m.pitch16=960;m.shape=64<<8;
 CHECK(sizeof(gr_grain_t)==(sizeof(void*)==8?64u:56u) && sizeof(gr_part_t)<=ENG_MEM_PART);
 for(unsigned part=0;part<4;part++){
  track_t *t=&trk[part];host_preset(t,ENGI_GRAIN,0);t->p[P_E0]=0;t->p[P_E1]=64;t->p[P_E2]=0;t->p[P_E6]=127;t->p[P_GTYPE]=part;t->p[P_GA]=0;
  gr_part_t *p=gr_of(t);memset(p,0,sizeof *p);gr_reset(p,0,0);for(unsigned i=0;i<GR_NIDX;i++)gr_build(p,t);
  for(unsigned val=0;val<128;val++){
   memset(p->g,0,sizeof p->g);gr_test_morph_mode[part]=1;gr_test_morph[part]=val;
   gr_spawn_one(p,t,0,0,500,&m,0,32767);CHECK(p->g[0].owner && p->g[0].window==128+val);
   gr_part_t frozen=*p;gr_test_morph[part]=(val+37)%128;gr_test_morph_mode[part]=val%3;gr_test_window=(val%3);
   unsigned blocks=0;
   while(p->g[0].owner){
    struct{uint32_t pre;int32_t v[CTL];uint32_t post;} a={.pre=0x1234,.post=0x5678},b=a;
    gr_run(p,&p->g[0],a.v,CTL);gr_run(&frozen,&frozen.g[0],b.v,CTL);
    CHECK(!memcmp(a.v,b.v,sizeof a.v) && a.pre==0x1234 && a.post==0x5678 && b.pre==0x1234 && b.post==0x5678);
    CHECK(p->g[0].window==128+val);CHECK(++blocks<1000);
   }
   gr_test_morph_mode[part]=1;gr_spawn_one(p,t,0,0,500,&m,0,32767);CHECK(p->g[0].owner && p->g[0].window==128+(val+37)%128);
  }
  /* Discrete grains also remain discrete when morphing is enabled mid-grain. */
  for(unsigned sel=0;sel<3;sel++){
   memset(p->g,0,sizeof p->g);gr_test_morph_mode[part]=0;gr_test_window=sel;
   gr_spawn_one(p,t,0,0,500,&m,0,32767);CHECK(p->g[0].owner && p->g[0].window==sel);
   gr_part_t frozen=*p;gr_test_morph_mode[part]=1;gr_test_morph[part]=32;gr_test_window=255;
   while(p->g[0].owner){
    int32_t a[CTL]={0},b[CTL]={0};gr_run(p,&p->g[0],a,CTL);gr_run(&frozen,&frozen.g[0],b,CTL);
    CHECK(!memcmp(a,b,sizeof a) && p->g[0].window==sel);
   }
   gr_spawn_one(p,t,0,0,500,&m,0,32767);CHECK(p->g[0].owner && p->g[0].window==160);
  }
  /* All invalid byte values, and OFF ignores an invalid morph value. */
  for(unsigned bad=128;bad<=255;bad++){
   memset(p->g,0,sizeof p->g);gr_test_morph_mode[part]=1;gr_test_morph[part]=bad;gr_spawn_one(p,t,0,0,500,&m,0,32767);CHECK(p->g[0].owner && p->g[0].window==0);
  }
  for(unsigned bad=2;bad<=255;bad++){
   memset(p->g,0,sizeof p->g);gr_test_morph_mode[part]=bad;gr_test_morph[part]=64;gr_spawn_one(p,t,0,0,500,&m,0,32767);CHECK(p->g[0].owner && p->g[0].window==0);
  }
  for(unsigned sel=0;sel<=255;sel++){
   memset(p->g,0,sizeof p->g);gr_test_morph_mode[part]=0;gr_test_morph[part]=255;gr_test_window=sel;gr_spawn_one(p,t,0,0,500,&m,0,32767);CHECK(p->g[0].owner && p->g[0].window==(sel<3?sel:0));
  }
 }
 printf("morph latching: %u checks passed; independent tracks, all values, invalid controls, no grain/part/track growth\n",checks);return 0;
}
