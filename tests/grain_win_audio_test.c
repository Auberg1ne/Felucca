/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#ifndef WIN_AUDIO_UI_SOURCE
#define WIN_AUDIO_UI_SOURCE "ui_test.c"
#endif
#include WIN_AUDIO_UI_SOURCE
#include "audio_stats.h"
int main(int argc,char**argv){if(argc!=2)return 2;FILE*f=fopen(argv[1],"wb");if(!f)return 2;unsigned checks=0;const unsigned wins[]={0,1,63,64,65,126,127};
 for(unsigned type=0;type<4;type++)for(unsigned w=0;w<7;w++){
  ui_power_on();song.master_q12=4096;audio_stats_t stats={0};
  for(unsigned t=0;t<4;t++){
   host_preset(&trk[t],ENGI_GRAIN,0);memset(trk[t].v,0,sizeof trk[t].v);trk[t].tail=0;
   trk[t].p[109]=wins[(w+t)%7];trk[t].p[P_GTYPE]=type;trk[t].p[P_E0]=3;
   trk[t].p[P_LEVEL]=100;trk[t].p[P_MUTE]=0;trk[t].p[P_ATK]=0;trk[t].p[P_SUS]=127;
   trk[t].p[P_DIST]=trk[t].p[P_ITYPE]=trk[t].p[P_CHOR]=trk[t].p[P_DLY]=trk[t].p[P_REV]=0;
   trk_note_on(&trk[t],60+t*2,110);
  }
  for(unsigned block=0;block<1000;block++){int32_t out[CTL*2];mix_block(out,CTL);if(fwrite(out,sizeof out,1,f)!=1)return 2;for(unsigned i=0;i<CTL*2;i++)audio_stats_push(&stats,out[i]);}
  if(!audio_stats_sounding(&stats,32,1)){fprintf(stderr,"silent final mixer: type=%u WIN case=%u\n",type,w);return 1;}checks++;
  printf("WIN audio type=%u case=%u peak=%.0f RMS=%.6f nonzero=%llu\n",type,w,stats.peak,audio_stats_rms(&stats),(unsigned long long)stats.nonzero);
 }
 fclose(f);printf("WIN four-simultaneous-track audio: %u sounding cases passed; no production WIN renderer\n",checks);return 0;
}
