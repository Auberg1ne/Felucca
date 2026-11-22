/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <time.h>
static uint64_t checks,cases,silent_old,silent_new,changed_short;
static double error_old,error_new;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"short envelope %u: %s\n",__LINE__,#x);return 1;}}while(0)
static int held(unsigned len,unsigned offset,unsigned rev,unsigned win,int32_t sample){
 gr_part_t p={0},old;gr_grain_t*g=&p.g[0];g->z=&SMP_ZONES[0];g->owner=1;g->window=128+win;g->rev=rev;g->a=g->b=sample;g->gain=32767;g->left=len;g->winc=UINT32_MAX/len;g->pos=1;old=p;
 unsigned nonzero=0,nzold=0,done=0,changed=0;int32_t previous=0,ref_previous=0;int maxjump=0,refjump=0;
 while(g->owner){
  struct{int32_t pre,v[CTL],post;}out={.pre=12345,.post=67890},baseline=out;
  unsigned n=done?CTL:CTL-offset,phase=g->wph,left=g->left,m=n<left?n:left;
  gr_run(&p,g,out.v,n);gr_run_block(&old,&old.g[0],baseline.v,n);
  CHECK(out.pre==12345&&out.post==67890);CHECK(!memcmp(&p,&old,sizeof p));
  if(len>32)CHECK(!memcmp(out.v,baseline.v,sizeof out.v));
  else changed|=memcmp(out.v,baseline.v,sizeof out.v)!=0;
  for(unsigned j=0;j<m;j++){
   int ref=mulq15(sample,mulq15(gr_morph_win(win,phase),32767));phase+=g->winc;
   int d=abs(out.v[j]-previous),rd=abs(ref-ref_previous);if(d>maxjump)maxjump=d;if(rd>refjump)refjump=rd;
   previous=out.v[j];ref_previous=ref;CHECK(out.v[j]>=-32768&&out.v[j]<=32767);
   double a=out.v[j]-ref,b=baseline.v[j]-ref;error_new+=a*a;error_old+=b*b;
   nonzero+=out.v[j]!=0;nzold+=baseline.v[j]!=0;
  }
  done++;
 }
 if(abs(previous)>maxjump)maxjump=abs(previous);if(abs(ref_previous)>refjump)refjump=abs(ref_previous);
 CHECK(nonzero>0);if(len<=32)CHECK(maxjump<=refjump+8);
 if(win==64&&sample==32767){silent_old+=!nzold;silent_new+=!nonzero;}
 changed_short+=changed;cases++;return 0;
}
static int decoder(void){
 const unsigned sources[]={0,2,3,13,14},lengths[]={16,17,31,32,33,64,128,441,2048},windows[]={0,32,64,96,127};
 const unsigned steps[]={16384,65536,131072};
 for(unsigned s=0;s<NELEM(sources);s++)for(unsigned rev=0;rev<2;rev++)for(unsigned li=0;li<NELEM(lengths);li++)for(unsigned wi=0;wi<NELEM(windows);wi++)for(unsigned st=0;st<NELEM(steps);st++){
  gr_part_t p={0},old;track_t t={0};gr_reset(&p,sources[s],gr_stamp(sources[s]));for(unsigned k=0;k<GR_NIDX;k++)gr_build(&p,&t);
  const smp_zone_t*z=gr_zone(sources[s],0);gr_grain_t*g=&p.g[0];g->z=z;g->owner=1;g->window=128+windows[wi];g->rev=rev;g->gain=32767;g->left=lengths[li];g->winc=UINT32_MAX/g->left;g->step=steps[st];
  if(rev){g->pos=z->n-2;gr_fill(&p,g,p.rb[0],0,g->pos+1);g->a=p.rb[0][g->pos-g->rlo];g->b=p.rb[0][g->pos+1-g->rlo];}
  else{g->a=gr_dec(z,0,&g->pred,&g->idx);g->b=gr_dec(z,1,&g->pred,&g->idx);g->pos=2;}
  old=p;unsigned block=0;
  while(g->owner){struct{int32_t pre,v[CTL],post;}out={.pre=12345,.post=67890},base=out;
   unsigned n=1+block++%CTL;gr_run(&p,g,out.v,n);gr_run_block(&old,&old.g[0],base.v,n);
   CHECK(!memcmp(&p,&old,sizeof p));CHECK(out.pre==12345&&out.post==67890);
   if(lengths[li]>32)CHECK(!memcmp(out.v,base.v,sizeof out.v));
  }
 }
 return 0;
}
/* Exercise the actual span-fitting/admission path, not just constructed grains. */
static int fitted(void){
 ui_power_on();usr_nz[0]=1;
 for(unsigned type=0;type<4;type++)for(unsigned frames=35;frames<=67;frames+=2)for(unsigned track=0;track<4;track++){
  usr_zone[0][0]=(smp_zone_t){.off=16,.n=frames,.ls=0,.le=frames-1,.rate=65536u,.root16=960,.lo=0,.hi=127};
  track_t*t=&trk[track];host_preset(t,ENGI_GRAIN,0);t->p[P_GTYPE]=type;t->p[P_E0]=5;t->p[P_E2]=127;t->p[P_E4]=0;t->p[P_E6]=0;t->p[P_GA]=127;t->p[P_GB]=64;
  gr_part_t p={0},old;gr_reset(&p,5,gr_stamp(5));for(unsigned k=0;k<GR_NIDX;k++)gr_build(&p,t);
  vmod_t m={0};m.pitch16=1152;m.shape=64<<8;
  gr_spawn_one(&p,t,0,0,441,&m,0,32767);gr_grain_t*g=&p.g[0];CHECK(g->owner&&g->left>=16&&g->left<=32);old=p;unsigned nonzero=0;
  while(g->owner){int32_t out[CTL]={0},ref[CTL]={0};gr_run(&p,g,out,CTL);gr_run_block(&old,&old.g[0],ref,CTL);CHECK(!memcmp(&p,&old,sizeof p));for(unsigned j=0;j<CTL;j++)nonzero+=out[j]!=0;}
  CHECK(nonzero>0);
 }
 return 0;
}
static volatile int64_t observed;
static uint64_t ns(void){struct timespec t;clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t);return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
static int bench(void){
 puts("length,window,reverse,kernel,repeat,ns_per_frame");
 for(unsigned leni=0;leni<5;leni++)for(unsigned win=0;win<4;win++)for(unsigned rev=0;rev<2;rev++)for(unsigned repeat=0;repeat<5;repeat++)for(unsigned method=0;method<2;method++){
  unsigned len=(unsigned[]){16,32,33,441,4410}[leni],loops=262144/len;gr_part_t p={0};
  int(*kernel)(gr_part_t*,gr_grain_t*,int32_t*,uint32_t)=method?gr_run:gr_run_block;
  uint64_t start=ns();for(unsigned k=0;k<loops;k++){
   gr_grain_t*g=&p.g[0];memset(g,0,sizeof *g);g->z=&SMP_ZONES[0];g->owner=1;g->window=(unsigned[]){128,192,255,224}[win];g->rev=rev;g->gain=32767;g->left=len;g->winc=UINT32_MAX/len;g->a=g->b=16000;g->pos=1;
   while(g->owner){int32_t out[CTL]={0};kernel(&p,g,out,CTL);for(unsigned j=0;j<CTL;j++)observed+=out[j];}
  }
  printf("%u,%u,%u,%u,%u,%.6f\n",len,win,rev,method,repeat,(double)(ns()-start)/(loops*len));
 }
 return observed==INT64_MIN;
}
int main(int argc,char**argv){
 if(argc==2&&!strcmp(argv[1],"--bench"))return bench();
 CHECK(sizeof(gr_grain_t)==64&&sizeof(gr_part_t)==7576);
 for(unsigned len=16;len<=128;len++)for(unsigned offset=0;offset<32;offset++)for(unsigned rev=0;rev<2;rev++)for(unsigned win=0;win<128;win++)for(unsigned sign=0;sign<2;sign++)if(held(len,offset,rev,win,sign?-32768:32767))return 1;
 CHECK(silent_old==306&&silent_new==0);CHECK(error_new<error_old);CHECK(!decoder());CHECK(!fitted());
 printf("short envelope: %llu cases; %llu checks; Hann silent %llu -> %llu / 7232; all128 windows; %llu changed short renders; squared error %.0f -> %.0f; decoder/state exact\n",(unsigned long long)cases,(unsigned long long)checks,(unsigned long long)silent_old,(unsigned long long)silent_new,(unsigned long long)changed_short,error_old,error_new);return 0;
}
