/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#ifndef SPARKLE_UI_SOURCE
#define SPARKLE_UI_SOURCE "ui_test.c"
#endif
#include SPARKLE_UI_SOURCE
#include <time.h>
int main(int argc,char**argv){
 if(argc!=3&&argc!=4)return 2;unsigned type=argc==4?atoi(argv[3]):3;if(type>3)return 2;ui_power_on();song.master_q12=4096;unsigned tracks=atoi(argv[2]);if(tracks!=1&&tracks!=4)return 2;
 for(unsigned k=0;k<4;k++){
  track_t*t=&trk[k];host_preset(t,ENGI_GRAIN,0);t->p[P_GTYPE]=type;t->p[P_GRC]=atoi(argv[1]);t->p[P_E0]=13;t->p[P_E2]=80;t->p[P_E3]=127;t->p[P_E4]=0;t->p[P_E5]=40;t->p[P_E6]=127;t->p[P_E7]=127;t->p[P_GB]=64;t->p[P_GCUT]=0;
  t->p[P_ATK]=t->p[P_DEC]=t->p[P_REL]=0;t->p[P_SUS]=127;t->p[P_LEVEL]=100;t->p[P_DIST]=t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=t->p[P_ITYPE]=0;t->p[P_MUTE]=k>=tracks;
  gr_part_t*g=gr_of(t);gr_reset(g,13,gr_stamp(13));for(unsigned i=0;i<GR_NIDX;i++)gr_build(g,t);gr_rng[k]=0x12345678+k;if(k<tracks)trk_note_on(t,48+2*k,100);
 }
 clock_t start=clock();uint64_t hash=0;unsigned peak=0;const unsigned blocks=32768;
 for(unsigned b=0;b<blocks;b++){int32_t out[CTL*2];mix_block(out,CTL);for(unsigned i=0;i<CTL*2;i++){unsigned a=out[i]<0?-out[i]:out[i];if(a>peak)peak=a;hash=hash*33+(uint32_t)out[i];}}
 printf("%.3f %u %llu\n",1e9*(clock()-start)/CLOCKS_PER_SEC/(blocks*CTL),peak,(unsigned long long)hash);return peak>0?0:1;
}
