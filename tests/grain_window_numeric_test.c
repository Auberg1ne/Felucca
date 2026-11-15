/* SPDX-License-Identifier: GPL-3.0-only */
/* Strict sanitizer target: production Tukey lookup, no unrelated DSP helpers. */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include "../firmware/src/grain_tukey.h"
static unsigned long checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"window line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
 CHECK(GR_TUKEY[0]==0 && GR_TUKEY[256]==0);
 for(unsigned i=0;i<=256;i++){
  double t=i/256.0,w=t<0.25?0.5*(1-cos(4*acos(-1)*t)):t>0.75?0.5*(1-cos(4*acos(-1)*(1-t))):1;
  CHECK(GR_TUKEY[i]>=0 && GR_TUKEY[i]<=32767);
  CHECK(GR_TUKEY[i]==GR_TUKEY[256-i]);CHECK(fabs(GR_TUKEY[i]-w*32767)<=0.501);
 }
 uint32_t r=1;
 for(unsigned n=0;n<2000000;n++){
  r=r*1664525u+1013904223u;uint32_t i=r>>24,f=(r>>9)&32767;
  int64_t prod=(int64_t)(GR_TUKEY[i+1]-GR_TUKEY[i])*f;
  int32_t ref=GR_TUKEY[i]+(int32_t)(prod>=0?prod/32768:-((-prod+32767)/32768));
  int32_t v=gr_tukey_win(r),a=GR_TUKEY[i],b=GR_TUKEY[i+1];
  CHECK(v==ref);CHECK(v>=0 && v<=32767);CHECK(v>=(a<b?a:b) && v<=(a>b?a:b));
  CHECK(abs(v-gr_tukey_win(0u-r))<=4);
 }
 CHECK(gr_tukey_win(0)==0 && gr_tukey_win(UINT32_MAX)==0);
 printf("window numerical: %lu checks passed; strict interpolation, symmetry, bounds\n",checks);return 0;
}
