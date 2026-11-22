/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "grain_stereo_filter_qualification.h"
#include <time.h>
int main(int argc,char**argv){
 if(argc!=3)return 2;int stereo=atoi(argv[1]),tracks=atoi(argv[2]);if(tracks!=1&&tracks!=4)return 2;
 ui_power_on();unsigned voices=tracks==1?3:8;static q_lr_state_t s[8];static voice_t ref[8];
 for(unsigned v=0;v<voices;v++){s[v].tone_a=ref[v].s[6]=GR_TONE_UNITY;s[v].filter=ref[v].s[7]=256u|(75u<<9);}
 vmod_t m={0};m.amp0=m.amp1=32767;int32_t target=gr_tone_coef(64);uint64_t hash=0;clock_t start=clock();
 const unsigned blocks=8192;
 for(unsigned b=0;b<blocks;b++)for(unsigned v=0;v<voices;v++){
  int32_t in[2][CTL],out[2][CTL]={{0}};
  for(unsigned i=0;i<CTL;i++){unsigned phase=b*CTL+i;in[0][i]=((phase+v*7)%64<32)?60000:-60000;in[1][i]=((phase+v*11)%128<64)?30000:-30000;}
  if(stereo)q_lr_block(&s[v],in,out,CTL,&m,target,25,127);
  else{gr_tone(&ref[v],in[0],CTL,target);gr_filter(&ref[v],in[0],out[0],CTL,&m,25,127);}
  for(unsigned i=0;i<CTL;i++)hash=hash*33+(uint32_t)out[0][i]+(uint32_t)out[1][i];
 }
 printf("%.3f %llu\n",1e9*(clock()-start)/CLOCKS_PER_SEC/(blocks*CTL),(unsigned long long)hash);return 0;
}
