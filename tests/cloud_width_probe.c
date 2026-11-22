/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <time.h>
int main(int argc,char**argv){
 if(argc!=2)return 2;unsigned mode=atoi(argv[1]);ui_power_on();static cloud_width_t s;
 uint32_t rng=1234567;double xx=0,ll=0,rr=0,lr=0;int64_t maxfold=0;int peak=0;clock_t begin=clock();
 const unsigned n=262144;
 for(unsigned i=0;i<n;i++){
  int x=(int16_t)noise32(&rng);int side=cw_step(&s,x,100,mode,16),l=x+side,r=x-side;
  int64_t fold=(int64_t)l+r-2*x;if(llabs(fold)>maxfold)maxfold=llabs(fold);
  if(abs(l)>peak)peak=abs(l);if(abs(r)>peak)peak=abs(r);
  if(i>1024){xx+=(double)x*x;ll+=(double)l*l;rr+=(double)r*r;lr+=(double)l*r;}
 }
 printf("mode=%u ns=%.3f bytes=%zu correlation=%.6f LR_balance_db=%.3f power_gain_db=%.3f fold_error=%lld peak=%d\n",mode,1e9*(clock()-begin)/CLOCKS_PER_SEC/n,sizeof s,lr/sqrt(ll*rr),10*log10(ll/rr),10*log10((ll+rr)/(2*xx)),(long long)maxfold,peak);return maxfold!=0;
}
