/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static uint64_t checks,cases;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"fidelity %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t windows[]={0,1,2,128,144,160,176,192,208,224,240,255};
static int run(unsigned len,unsigned offset,unsigned rev,uint32_t step,int32_t sample,unsigned win,unsigned method,unsigned cap,FILE *metrics){
 gr_part_t a={0},b={0},base={0};smp_zone_t z=SMP_ZONES[0];
 /* No decode needed for static-envelope metric; rate=0 holds the signed input. */
 gr_grain_t *g=&a.g[0];g->z=&z;g->owner=1;g->window=win;g->rev=rev;g->step=step;g->gain=32767;g->a=g->b=sample;
 g->left=len;g->winc=UINT32_MAX/len;g->pos=rev?1024:2;
 if(rev){g->rlo=0;for(unsigned i=0;i<GR_RB;i++)a.rb[0][i]=sample;g->pos=step?100:1;}
 b=base=a;double error=0,energy=0,peak=0,jump=0;int32_t prev=0;unsigned nonzero=0,done=0;
 while(g->owner){
  unsigned n=done?CTL:CTL-offset;struct{uint32_t pre;int32_t v[CTL];uint32_t post;} out={.pre=0x1234,.post=0x5678},ref=out,old=out;
  gr_run_fidelity(&a,&a.g[0],out.v,n,method,cap);
  for(unsigned j=0;j<n && b.g[0].left;j++)gr_run(&b,&b.g[0],ref.v+j,1);
  gr_run(&base,&base.g[0],old.v,n);
  CHECK(out.pre==0x1234 && out.post==0x5678 && ref.pre==0x1234 && ref.post==0x5678 && old.pre==0x1234 && old.post==0x5678);
  CHECK(!memcmp(&a,&b,sizeof a) && !memcmp(&a,&base,sizeof a)); /* all decoder/phase/cache state */
  if(method==0 || method>2 || (method==1 && cap!=4 && cap!=8 && cap!=16))CHECK(!memcmp(out.v,old.v,sizeof out.v));
  if(method==2)CHECK(!memcmp(out.v,ref.v,sizeof out.v));
  unsigned m=len-done<n?len-done:n;
  if(method==1 && (cap==4 || cap==8 || cap==16)){
   unsigned j=0;while(j<m){CHECK(out.v[j]==ref.v[j]);unsigned k=len<=16?1:len<=32?2:len<=64?4:len<=256?8:16;if(k>cap)k=cap;
    unsigned left=len-done-j;if(left<=2*k){unsigned h=left/2;if(!h)h=1;if(k>h)k=h;}if(k>n-j)k=n-j;j+=k;}
  }
  for(unsigned i=0;i<m;i++){double d=(double)out.v[i]-ref.v[i];error+=d*d;energy+=(double)out.v[i]*out.v[i];if(abs(out.v[i])>peak)peak=abs(out.v[i]);if(abs(out.v[i]-prev)>jump)jump=abs(out.v[i]-prev);prev=out.v[i];nonzero+=out.v[i]!=0;}
  done+=m;CHECK(done<=len);
 }
 if(!step && sample==32767){if(len==1)CHECK(!nonzero);else if(method==2 || (method==1 && (cap==4 || cap==8 || cap==16)))CHECK(nonzero>0);}
 if(abs(prev)>jump)jump=abs(prev);cases++;
 if(metrics)fprintf(metrics,"%u,%u,%u,%u,%u,%u,%.9f,%.9f,%.0f,%.0f,%u\n",len,offset,rev,win,method,cap,sqrt(error/len)/32767,sqrt(energy/len)/32767,peak,jump,!nonzero);
 return 0;
}
static int real_cache(void){
 gr_part_t indexed={0};track_t t={0};gr_reset(&indexed,13,0);for(unsigned i=0;i<GR_NIDX;i++)gr_build(&indexed,&t);
 const smp_zone_t *z=gr_zone(13,0);CHECK(z->n>172868);
 for(unsigned w=0;w<NELEM(windows);w++)for(unsigned rev=0;rev<2;rev++)for(unsigned method=0;method<5;method++){
  gr_part_t a=indexed,b,base;gr_grain_t *g=&a.g[0];g->owner=1;g->z=z;g->window=windows[w];g->rev=rev;g->gain=32767;g->left=86432;g->winc=UINT32_MAX/g->left;g->step=131072;
  if(rev){g->pos=z->n-2;gr_fill(&a,g,a.rb[0],0,g->pos+1);g->a=a.rb[0][g->pos-g->rlo];g->b=a.rb[0][g->pos+1-g->rlo];}
  else{g->a=gr_dec(z,0,&g->pred,&g->idx);g->b=gr_dec(z,1,&g->pred,&g->idx);g->pos=2;}
  b=base=a;uint64_t energy=0;unsigned blocks=0,refills=0;
  while(g->owner){
   struct{uint32_t pre;int32_t v[CTL];uint32_t post;} out={.pre=0x1234,.post=0x5678},ref=out,old=out;
   uint32_t oldlo=g->rlo;gr_run_fidelity(&a,g,out.v,CTL,method==4?2:method?1:0,method==1?4:method==2?8:16);
   for(unsigned j=0;j<CTL && b.g[0].left;j++)gr_run(&b,&b.g[0],ref.v+j,1);
   gr_run(&base,&base.g[0],old.v,CTL);refills+=oldlo!=g->rlo;
   CHECK(!memcmp(&a,&b,sizeof a) && !memcmp(&a,&base,sizeof a));CHECK(out.pre==0x1234 && out.post==0x5678);
   if(!method)CHECK(!memcmp(out.v,old.v,sizeof out.v));if(method==4)CHECK(!memcmp(out.v,ref.v,sizeof out.v));
   for(unsigned j=0;j<CTL;j++)energy+=llabs(out.v[j]);CHECK(++blocks<=2701);
  }
  CHECK(energy>0 && (!rev || refills>1000));cases++;
 }
 return 0;
}
int main(int argc,char **argv){
 FILE *f=argc>1?fopen(argv[1],"w"):0;if(argc>1)CHECK(f);
 if(f)fprintf(f,"length,offset,reverse,window,method,cap,error_RMS,output_RMS,peak,max_jump,silent\n");
 CHECK(sizeof(gr_grain_t)==64 && sizeof(gr_part_t)==7320 && sizeof(track_t)==1840);
 for(unsigned len=1;len<=128;len++)for(unsigned off=0;off<32;off++)for(unsigned rev=0;rev<2;rev++)for(unsigned w=0;w<NELEM(windows);w++)for(unsigned method=0;method<5;method++)
  if(run(len,off,rev,0,32767,windows[w],method==4?2:method?1:0,method==1?4:method==2?8:16,f))return 1;
 const unsigned longlen[]={129,441,4410,86432};
 for(unsigned li=0;li<4;li++)for(unsigned w=0;w<NELEM(windows);w++)for(unsigned method=0;method<5;method++)
  if(run(longlen[li],13,0,0,32767,windows[w],method==4?2:method?1:0,method==1?4:method==2?8:16,f))return 1;
 /* Signed source extremes and bounded source advancement, both directions. */
 for(unsigned rev=0;rev<2;rev++)for(unsigned rate=0;rate<4;rate++)for(unsigned w=0;w<NELEM(windows);w++)for(unsigned method=0;method<5;method++)for(int sign=-1;sign<=1;sign+=2)
  if(run(17,31,rev,(uint32_t[]){256,32768,65536,131072}[rate],sign<0?-32768:32767,windows[w],method==4?2:method?1:0,method==1?4:method==2?8:16,0))return 1;
 for(unsigned bad=3;bad<=255;bad++)if(run(33,5,0,0,32767,192,bad,4,0))return 1;
 for(unsigned bad=0;bad<=255;bad++)if(bad!=4 && bad!=8 && bad!=16)if(run(33,5,0,0,32767,192,1,bad,0))return 1;
 if(real_cache())return 1;
 if(f)CHECK(!fclose(f));printf("fidelity kernel: %llu checks, %llu cases passed; full decoder-state identity, offsets, signed extremes, strict sanitizers\n",(unsigned long long)checks,(unsigned long long)cases);return 0;
}
