/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks,changed;static gr_part_t indexed,p,reference,baseline;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"CHAOS %u: %s\n",__LINE__,#x);return 1;}}while(0)
static uint64_t add(uint64_t h,uint32_t x){return (h^x)*UINT64_C(1099511628211);}
static int scheduler(track_t*t,unsigned mode,uint64_t *signature){
 gr_part_t*P=gr_of(t);*P=indexed;memset(t->v,0,sizeof t->v);voice_t*v=&t->v[0];v->active=1;v->s[0]=0;v->s[1]=0;t->p[P_GTYPE]=2;t->p[P_GA]=127;t->p[P_GB]=64;t->p[P_E3]=127;t->p[P_E2]=0;t->p[P_E4]=0;t->p[P_E5]=40;t->p[P_E6]=127;t->p[P_E7]=127;t->p[P_GCUT]=0;
 vmod_t m={0};m.pitch16=gr_zone(P->src-1,0)->root16;m.shape=64<<8;m.amp0=m.amp1=32767;gr_rng[0]=0x12345678;uint64_t h=0;
 for(unsigned b=0;b<4096;b++){
  t->p[P_GRC]=mode==2?(int[]){0,100,0,25,50,100,0,50}[b/128%8]:mode?100:0;
  struct{int32_t pre,out[CTL],post;}out={.pre=12345,.post=67890};grain_render(t,v,out.out,CTL,&m);CHECK(out.pre==12345&&out.post==67890);
  h=add(h,gr_rng[0]);h=add(h,v->s[1]);h=add(h,v->ph[1]);h=add(h,P->head[0].sample);h=add(h,P->head[0].frac);
  unsigned active=0,load=0;for(unsigned j=0;j<GR_NG;j++){gr_grain_t*g=&P->g[j];h=add(h,g->owner);if(!g->owner)continue;active++;load+=g->step<<g->rev;h=add(h,g->step);h=add(h,g->left);h=add(h,g->wph);CHECK(g->step<=GR_STEP_MAX);}
  CHECK(active<=GR_NG&&load<=GR_LOAD_MAX);for(unsigned j=0;j<CTL;j++)CHECK(out.out[j]>=-1000000&&out.out[j]<=1000000);
 }
 *signature=h;return 0;
}
int main(void){
 ui_power_on();track_t*t=&trk[0];const unsigned sources[]={0,2,3,13,14};CHECK(sizeof(gr_grain_t)==64&&sizeof(gr_part_t)==7576&&sizeof(track_t)==1840);
 for(unsigned s=0;s<8;s++){
  unsigned source=s<5?sources[s]:5;
  if(s>=5){usr_nz[0]=1;usr_zone[0][0]=*gr_zone(13,0);usr_zone[0][0].n=(unsigned[]){32,257,65535}[s-5];}
  host_preset(t,ENGI_GRAIN,0);t->p[P_E0]=source;gr_reset(&indexed,source,gr_stamp(source));for(unsigned k=0;k<GR_NIDX;k++)gr_build(&indexed,t);
  const smp_zone_t*z=gr_zone(source,0);t->p[P_GTYPE]=2;t->p[P_E2]=0;t->p[P_E4]=0;t->p[P_E5]=40;t->p[P_GB]=64;t->v[0].fine=0;
  for(unsigned seed=1;seed<=64;seed++)for(unsigned revcase=0;revcase<2;revcase++)for(unsigned poscase=0;poscase<3;poscase++){
   t->p[P_E1]=(int[]){0,64,127}[poscase];t->p[P_E6]=revcase?127:0;
   for(unsigned ci=0;ci<7;ci++){
    int c=(int[]){0,1,25,50,100,32767,-32768}[ci];p=indexed;t->p[P_GRC]=c;uint32_t r=seed*0x9e3779b9u;gr_rng[0]=r;uint32_t r1=noise32(&r),r2=noise32(&r);
    vmod_t m={0};m.pitch16=z->root16-(s>=5?192:0);m.shape=64<<8;gr_spawn_one(&p,t,0,0,1000,&m,0,32767);gr_grain_t*g=&p.g[0];CHECK(g->owner==1&&gr_rng[0]==r);
    unsigned span=(uint32_t)(((uint64_t)441u*g->step+(g->rev?65535u:0u))>>16),spread=gr_scale(z->n,40u*516u);
    if(span>z->n-3u)span=z->n-3u; /* conservative pre-fit span, not rounded fitted length */
    int64_t raw=(int64_t)gr_head_anchor(z->n,t->p[P_E1]<<8)+gr_scale(spread,r2&65535u)-(spread>>1);
    unsigned amount=c<0?0:c>100?100:c;unsigned width=(uint32_t)(((uint64_t)z->n*(amount*amount*32768u/10000u))>>16);
    int64_t offset=(int64_t)(((uint64_t)width*(r2>>16))>>16)-(width>>1);CHECK(llabs(offset)<=(int64_t)(z->n/4+1));raw+=offset;
    int64_t lo=g->rev?span:0,hi=g->rev?z->n-2:z->n-3-span;int64_t expected=raw<lo?lo:raw>hi?hi:raw;
    CHECK(g->pos==(uint32_t)expected+(g->rev?0:2));CHECK(g->rev==(((r1>>16)&255u)<(unsigned)t->p[P_E6]));
    if(!ci)baseline=p;else{CHECK(g->step==baseline.g[0].step&&g->left==baseline.g[0].left&&g->gain==baseline.g[0].gain);CHECK(!memcmp(p.head,baseline.head,sizeof p.head));changed+=g->pos!=baseline.g[0].pos;}
    /* Live changes cannot rewrite a captured grain's decoder trajectory. */
    reference=p;t->p[P_GRC]=100-amount;struct{int32_t pre,out[CTL],post;}a={.pre=12345,.post=67890},b=a;
    gr_run(&p,g,a.out,CTL);gr_run(&reference,&reference.g[0],b.out,CTL);CHECK(!memcmp(&p,&reference,sizeof p)&&!memcmp(&a,&b,sizeof a));CHECK(a.pre==12345&&a.post==67890);
   }
  }
  t->p[P_E1]=64;uint64_t a,b,c;CHECK(!scheduler(t,0,&a));CHECK(!scheduler(t,1,&b));CHECK(!scheduler(t,2,&c));CHECK(a==b&&a==c);
 }
 CHECK(changed>1000);printf("CHAOS kernel: %u checks; %u position changes; scheduler/RNG/gate/load signatures exact at C0/C100/live\n",checks,changed);return 0;
}
