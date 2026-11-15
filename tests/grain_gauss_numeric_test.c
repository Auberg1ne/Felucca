/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../firmware/src/grain_gauss.h"
static uint64_t checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Gaussian line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 CHECK(GR_GAUSS[0]==0 && GR_GAUSS[256]==0 && GR_GAUSS[128]==32767);
 for(unsigned i=0;i<=256;i++){
  double t=i/256.0,w=(exp(-9*(2*t-1)*(2*t-1))-exp(-9))/(1-exp(-9));
  CHECK(GR_GAUSS[i]>=0 && GR_GAUSS[i]<=32767);CHECK(GR_GAUSS[i]==GR_GAUSS[256-i]);CHECK(fabs(GR_GAUSS[i]-w*32767)<=.501);
 }
 /* Exhaust all 8,388,608 distinct lookup cells/fractions; low nine bits are
  * discarded, so test both ends of every 512-phase equivalence class. */
 for(unsigned i=0;i<256;i++)for(unsigned f=0;f<32768;f++){
  uint32_t p=(i<<24)|(f<<9);int64_t prod=(int64_t)(GR_GAUSS[i+1]-GR_GAUSS[i])*f;
  int32_t ref=GR_GAUSS[i]+(int32_t)(prod>=0?prod/32768:-((-prod+32767)/32768)),v=gr_gauss_win(p);
  CHECK(v==ref);CHECK(v>=0 && v<=32767);CHECK(gr_gauss_win(p|511u)==v);
  CHECK(abs(v-gr_gauss_win(0u-p))<=4);
 }
 CHECK(gr_gauss_win(0)==0 && gr_gauss_win(UINT32_MAX)==0);
 printf("Gaussian numerical: %llu checks passed; all effective Q32 phases, symmetry, endpoints, center and wide reference\n",(unsigned long long)checks);return 0;
}
