/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static uint64_t checks,wraps;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"DRIFT DSP %u: %s\n",__LINE__,#x);return 1;}}while(0)
static int64_t bound(int64_t x,int64_t lo,int64_t hi){return x<lo?lo:x>hi?hi:x;}
static gr_drift_t reference(gr_drift_t d,int control,unsigned n){
 n=n>32?32:n;int64_t c=bound(control,0,100);if(c)d.phase=(uint32_t)((uint64_t)d.phase+(uint64_t)n*4869);
 int64_t depth=c*c*32768/10000,target=(int64_t)sine_i(d.phase)*depth/512;
 int64_t current=bound(d.displacement,-2097152,2097152),step=96*n;d.displacement=current+bound(target-current,-step,step);return d;
}
int main(void){
 const uint32_t phases[]={0,1,0x3fffffff,0x40000000,0x7fffffff,0x80000000,0xbfffffff,0xc0000000,UINT32_MAX-1,UINT32_MAX};
 const int values[]={0,1,25,50,100,101,-1,INT32_MIN,INT32_MAX};
 const int disps[]={0,1,-1,GR_DRIFT_LIMIT,-GR_DRIFT_LIMIT,INT32_MIN,INT32_MAX};
 for(unsigned p=0;p<NELEM(phases);p++)for(unsigned c=0;c<NELEM(values);c++)for(unsigned q=0;q<NELEM(disps);q++)for(unsigned ni=0;ni<7;ni++){
  unsigned n=(unsigned[]){0,1,7,16,31,32,UINT32_MAX}[ni];gr_drift_t d={phases[p],disps[q]},expected=reference(d,values[c],n);gr_drift_update(&d,values[c],n);CHECK(!memcmp(&d,&expected,sizeof d));CHECK(abs(d.displacement)<=GR_DRIFT_LIMIT);
 }
 /* Every legal control at every sine table point: exact quadratic target. */
 for(unsigned i=0;i<1024;i++)for(unsigned c=0;c<=100;c++){
  uint32_t phase=i<<22;int64_t depth=(int64_t)c*c*32768/10000,target=(int64_t)sine_i(phase)*depth/512;
  gr_drift_t a={phase-(c?32u*4869u:0u),(int32_t)target};gr_drift_update(&a,c,32);
  CHECK(a.phase==phase&&a.displacement==target);CHECK(llabs(target)<=GR_DRIFT_LIMIT);
 }
 /* Multiple full wraps, mixed fragments and control reversals, no reset/snap. */
 gr_drift_t d={0};uint64_t frames=0;unsigned maxdelta=0;
 for(unsigned b=0;b<300000;b++){
  unsigned n=b%32+1;int old=d.displacement;uint32_t phase=d.phase;gr_drift_update(&d,100,n);frames+=n;CHECK(d.phase==(uint32_t)(frames*4869));wraps+=d.phase<phase;
  unsigned delta=abs(d.displacement-old);if(delta>maxdelta)maxdelta=delta;CHECK(delta<=96*n);CHECK(abs(d.displacement)<=GR_DRIFT_LIMIT);
 }
 CHECK(wraps>=5);uint32_t phase=d.phase;unsigned tail=0;while(d.displacement){int old=d.displacement;gr_drift_update(&d,0,1);CHECK(d.phase==phase);CHECK(abs(d.displacement)<=abs(old));CHECK(abs(d.displacement-old)<=96);CHECK(++tail<=21846);}
 for(unsigned b=0;b<100000;b++){int c=(int[]){0,100,0,25,100,50,0,100}[b/83%8];int old=d.displacement;gr_drift_t expected=reference(d,c,32);gr_drift_update(&d,c,32);CHECK(!memcmp(&d,&expected,sizeof d));CHECK(abs(d.displacement-old)<=3072);}
 /* Wide mapping reference, including corrupt state and adversarial domains. */
 const uint32_t lengths[]={0,1,3,8,16,32,257,65535,65536,275098,UINT32_MAX};
 for(unsigned l=0;l<NELEM(lengths);l++)for(unsigned s=0;s<NELEM(lengths);s++)for(unsigned q=0;q<NELEM(disps);q++){
  uint32_t n=lengths[l],span=lengths[s];gr_drift_t a={0,disps[q]};int64_t room=n>3&&span<n-3?(uint64_t)n-3-span:0,lim=room/8;
  int64_t expected=bound(bound(a.displacement,-2097152,2097152)*n/16777216,-lim,lim);CHECK(gr_drift_offset(&a,n,span)==expected);
 }
 ui_power_on();track_t*t=&trk[0];static gr_part_t indexed,P;const unsigned srcs[]={0,2,3,13,14};unsigned admitted=0,reversed=0;
 for(unsigned source=0;source<8;source++){
  unsigned src=source<5?srcs[source]:5;if(source>=5){usr_nz[0]=1;usr_zone[0][0]=*gr_zone(13,0);usr_zone[0][0].n=(unsigned[]){32,257,65535}[source-5];}
  host_preset(t,ENGI_GRAIN,0);t->p[P_E0]=src;t->p[P_GTYPE]=1;t->p[P_GA]=127;t->p[P_GB]=0;t->p[P_E5]=0;t->v[0].fine=0;gr_reset(&indexed,src,gr_stamp(src));for(unsigned k=0;k<GR_NIDX;k++)gr_build(&indexed,t);const smp_zone_t*z=gr_zone(src,0);
  for(unsigned pc=0;pc<3;pc++)for(unsigned rev=0;rev<2;rev++)for(unsigned size=0;size<2;size++)for(unsigned q=0;q<NELEM(disps);q++){
   P=indexed;t->p[P_E1]=(int[]){0,64,127}[pc];t->p[P_E2]=size?127:0;t->p[P_E4]=0;t->p[P_E6]=rev?127:0;P.drift[0].displacement=disps[q];vmod_t m={0};m.pitch16=z->root16-192;m.shape=64<<8;gr_rng[0]=rev?1u:0x12345678u;
   uint32_t rng=gr_rng[0];noise32((int32_t*)&rng);noise32((int32_t*)&rng);gr_spawn_one(&P,t,0,0,1000,&m,0,32767);CHECK((uint32_t)gr_rng[0]==rng);gr_grain_t*g=&P.g[0];if(!g->owner)continue;admitted++;CHECK(g->rev==rev);reversed+=g->rev;
   unsigned len=(441u*pow2_q16(t->p[P_E2]*8))>>16;len=len*(128u+127u*3u)/128u;uint64_t span=((uint64_t)len*g->step+(g->rev?65535:0))>>16;if(span>z->n-3)span=z->n-3;
   int64_t offset=bound(disps[q],-2097152,2097152)*z->n/16777216;int64_t lim=((int64_t)z->n-3-span)/8;offset=bound(offset,-lim,lim);
   int64_t raw=(int64_t)gr_head_anchor(z->n,t->p[P_E1]<<8)+offset,lo=g->rev?span:0,hi=g->rev?z->n-2:z->n-3-span;
   CHECK(g->pos==(uint32_t)bound(raw,lo,hi)+(g->rev?0:2));uint32_t step=g->step;gr_drift_t captured=P.drift[0];
   while(g->owner){struct{int32_t pre,out[CTL],post;}out={.pre=12345,.post=67890};gr_drift_update(&P.drift[0],0,CTL);gr_run(&P,g,out.out,CTL);CHECK(g->step==step);CHECK(out.pre==12345&&out.post==67890);}
  }
 }
 CHECK(admitted>500&&reversed>250);printf("DRIFT DSP: %llu checks; %llu wraps; max smooth delta=%u Q24; %u fitted forward/reverse grains; wide arithmetic and live tails passed\n",(unsigned long long)checks,(unsigned long long)wraps,maxdelta,admitted);return 0;
}
