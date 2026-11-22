/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks,accents,chosen_count[6];
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"sparkle %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 ui_power_on();track_t*t=&trk[0];static gr_part_t indexed,p;const unsigned src[]={0,2,3,13,14};
 CHECK(sizeof(gr_grain_t)==64 && sizeof(gr_part_t)==7320 && sizeof(track_t)==1840);
 for(unsigned s=0;s<5;s++){
  host_preset(t,ENGI_GRAIN,0);t->p[P_E0]=src[s];gr_reset(&indexed,src[s],gr_stamp(src[s]));
  for(unsigned i=0;i<GR_NIDX;i++)gr_build(&indexed,t);
  const smp_zone_t*z=gr_zone(src[s],0);t->p[P_E1]=64;t->p[P_E2]=0;t->p[P_E4]=0;t->p[P_E5]=0;t->p[P_E6]=0;t->v[0].fine=0;
  for(unsigned type=0;type<4;type++)for(unsigned seed=1;seed<=256;seed++){
   uint32_t r=seed*0x9e3779b9u;noise32(&r);uint32_t r2=noise32(&r);unsigned neutral=0;
   for(unsigned ci=0;ci<6;ci++){
    int c=(int[]){0,1,25,50,100,32767}[ci];p=indexed;t->p[P_GTYPE]=type;t->p[P_GRC]=c;t->p[P_GA]=64;t->p[P_GB]=64;
    vmod_t m={0};m.pitch16=z->root16-192;gr_rng[0]=seed*0x9e3779b9u;
    gr_spawn_one(&p,t,0,0,1000,&m,0,32767);gr_grain_t*g=&p.g[0];CHECK(g->owner==1);CHECK(gr_rng[0]==r);
    if(!ci)neutral=g->step;
    unsigned chosen=type==3 && c>0 && (r2>>16)*200u<(unsigned)(c>100?100:c)*65536u;
    CHECK(g->step==(chosen?neutral*2:neutral));CHECK(g->step<=GR_STEP_MAX);accents+=chosen;chosen_count[ci]+=chosen;
    unsigned captured=g->step;t->p[P_GRC]=100-c%101;
    while(g->owner){struct{int32_t pre,out[CTL],post;}b={.pre=12345,.post=67890};gr_run(&p,g,b.out,CTL);CHECK(b.pre==12345&&b.post==67890);CHECK(g->step==captured);}
   }
  }
  /* Existing SHIMMER harmony keeps exactly its interval, gain and source state. */
  static gr_part_t harmony;
  vmod_t hm={0};hm.pitch16=z->root16-192;t->p[P_GTYPE]=3;t->p[P_E4]=0;t->p[P_E6]=0;
  p=indexed;t->p[P_GRC]=0;gr_rng[0]=12345;gr_spawn_one(&p,t,0,0,1000,&hm,192,16512);harmony=p;uint32_t hr=gr_rng[0];
  p=indexed;t->p[P_GRC]=100;gr_rng[0]=12345;gr_spawn_one(&p,t,0,0,1000,&hm,192,16512);CHECK(!memcmp(&p,&harmony,sizeof p)&&gr_rng[0]==hr);
  /* Pitch, RAND/reverse, UNISON fine, source fitting and decode-load cap. */
  for(unsigned pitch=0;pitch<3;pitch++)for(unsigned reverse=0;reverse<2;reverse++)for(unsigned size=0;size<2;size++){
   p=indexed;t->p[P_GTYPE]=3;t->p[P_GRC]=100;t->p[P_E4]=(int[]){-24,0,24}[pitch];t->p[P_E6]=reverse?127:0;t->p[P_E2]=size?127:0;t->v[0].fine=254;
   vmod_t m={0};m.pitch16=z->root16;gr_rng[0]=0x12345678;
   for(unsigned k=0;k<32;k++)gr_spawn(&p,t,0,0,64,&m);
   unsigned load=0,count=0;for(unsigned k=0;k<GR_NG;k++)if(p.g[k].owner){count++;load+=p.g[k].step<<p.g[k].rev;CHECK(p.g[k].step<=GR_STEP_MAX);}
   CHECK(count<=GR_NG&&load<=GR_LOAD_MAX);
   for(unsigned k=0;k<GR_NG;k++)while(p.g[k].owner){struct{int32_t pre,out[CTL],post;}b={.pre=12345,.post=67890};gr_run(&p,&p.g[k],b.out,CTL);CHECK(b.pre==12345&&b.post==67890);}
  }
 }
 CHECK(accents>1000);CHECK(chosen_count[0]==0 && chosen_count[1]<chosen_count[2] && chosen_count[2]<chosen_count[3] && chosen_count[3]<chosen_count[4] && chosen_count[4]==chosen_count[5]);
 printf("Accent counts C0/1/25/50/100: %u/%u/%u/%u/%u\n",chosen_count[0],chosen_count[1],chosen_count[2],chosen_count[3],chosen_count[4]);printf("SPARKLE kernel: %u checks; %u octave accents; RNG draw count, latching, caps and canaries passed\n",checks,accents);return 0;
}
