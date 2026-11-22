/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <time.h>
int main(int argc,char**argv){
 if(argc!=2)return 2;unsigned stereo=atoi(argv[1]);ui_power_on();static gr_part_t p;
 for(unsigned j=0;j<GR_NG;j++){
  gr_grain_t*g=&p.g[j];g->z=gr_zone(13,0);g->owner=1;g->a=g->b=16000;g->gain=9000;g->left=UINT32_MAX;g->winc=32768;g->window=192;
#if FELUCCA_GRAIN_STEREO_PROTOTYPE
  p.pan[j]=(j&1)?48:-48;
#endif
 }
 uint64_t hash=0;clock_t start=clock();const unsigned blocks=32768;
 for(unsigned b=0;b<blocks;b++){
  int32_t mid[CTL]={0},side[CTL]={0};
  for(unsigned j=0;j<GR_NG;j++){
#if FELUCCA_GRAIN_STEREO_PROTOTYPE
   if(stereo)gr_run_stereo_probe(&p,&p.g[j],mid,side,CTL);else
#endif
   gr_run(&p,&p.g[j],mid,CTL);
  }
  for(unsigned i=0;i<CTL;i++)hash=hash*33+(uint32_t)mid[i]+(uint32_t)side[i];
 }
 printf("%.3f %llu\n",1e9*(clock()-start)/CLOCKS_PER_SEC/(blocks*CTL*GR_NG),(unsigned long long)hash);return 0;
}
