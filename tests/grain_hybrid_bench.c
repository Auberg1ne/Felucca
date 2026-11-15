/* SPDX-License-Identifier: GPL-3.0-only */
/* Host-only microbenchmark: common reset/initial seek + actual grain kernels. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <time.h>
static volatile int64_t observed;
static uint64_t ns(void){struct timespec t;clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t);return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
static unsigned evals(unsigned len,unsigned method,unsigned cap,unsigned threshold,unsigned optimize){
 if(method==3)method=len<=threshold?2:1;
 if(method==2)return len;unsigned left=len,calls=0,ones=0;
 while(left){unsigned n=left<32?left:32,done=0;
  while(done<n){unsigned k=n-done;if(method==1){k=len<=16?1:len<=32?2:len<=64?4:len<=256?8:16;if(k>cap)k=cap;if(left<=2*k){unsigned h=left/2;if(!h)h=1;if(k>h)k=h;}if(k>n-done)k=n-done;}
   done+=k;left-=k;calls++;ones+=k==1;
  }
 }return 2*calls-1-(optimize && ones?ones-1:0);
}
int main(int argc,char **argv){
 FILE *f=argc>1?fopen(argv[1],"w"):stdout;if(!f)return 2;
 fprintf(f,"length,window,reverse,method,cap,threshold,optimize,repeat,ns_per_frame,window_evaluations,table_lookups\n");
 gr_part_t p={0};track_t t={0};gr_reset(&p,13,0);for(unsigned i=0;i<GR_NIDX;i++)gr_build(&p,&t);const smp_zone_t *z=gr_zone(13,0);
 const unsigned lengths[]={16,32,64,128,129,441,4410};
 const unsigned methods[]={0,1,1,2,3,3,3,3,3,3};
 const unsigned caps[]={8,4,8,8,4,8,4,8,4,8};
 const unsigned thresholds[]={32,32,32,32,32,32,64,64,128,128};
 for(unsigned li=0;li<NELEM(lengths);li++)for(unsigned win=0;win<4;win++)for(unsigned rev=0;rev<2;rev++)for(unsigned policy=0;policy<10;policy++)for(unsigned opt=0;opt<(methods[policy]==3?2u:1u);opt++)for(unsigned repeat=0;repeat<5;repeat++){
  unsigned len=lengths[li],loops=262144/len;if(loops<8)loops=8;unsigned kind=methods[policy],cap=caps[policy],threshold=thresholds[policy];
  uint64_t start=ns();for(unsigned iter=0;iter<loops;iter++){
   gr_grain_t *g=&p.g[0];memset(g,0,sizeof *g);g->z=z;g->owner=1;g->window=(unsigned[]){128,192,255,224}[win];g->rev=rev;g->gain=32767;g->left=len;g->winc=UINT32_MAX/len;g->step=65536;
   if(rev){g->pos=z->n-2;gr_fill(&p,g,p.rb[0],0,g->pos+1);g->a=p.rb[0][g->pos-g->rlo];g->b=p.rb[0][g->pos+1-g->rlo];}
   else{g->a=gr_dec(z,0,&g->pred,&g->idx);g->b=gr_dec(z,1,&g->pred,&g->idx);g->pos=2;}
   while(g->owner){int32_t out[CTL]={0};gr_run_envelope(&p,g,out,CTL,kind,cap,threshold,opt);for(unsigned i=0;i<CTL;i++)observed+=out[i];}
   observed+=g->pos+g->pred+g->idx;
  }
  uint64_t elapsed=ns()-start;unsigned calls=evals(len,kind,cap,threshold,opt);
  fprintf(f,"%u,%u,%u,%u,%u,%u,%u,%u,%.6f,%u,%u\n",len,(unsigned[]){128,192,255,224}[win],rev,kind,cap,threshold,opt,repeat,(double)elapsed/(loops*len),calls,calls*(win==3?2:1));
 }
 if(f!=stdout)fclose(f);return observed==INT64_MIN;
}
