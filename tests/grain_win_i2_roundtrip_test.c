/* SPDX-License-Identifier: GPL-3.0-only */
#define PERSISTENCE_TEST_NO_MAIN 1
#include "persistence_test.c"
#include "audio_stats.h"
int main(int argc,char**argv){
 if(argc!=5)return 2;unsigned value=atoi(argv[1]),type=atoi(argv[2]),route=atoi(argv[3]);if(value>127||type>3||route>2)return 2;
 reset();song.master_q12=4096;
 for(unsigned c=0;c<4;c++){
  track_t*t=&trk[c];set_engine_of(t,ENGI_GRAIN);t->engine=ENGI_GRAIN;t->p[109]=value;t->p[P_GTYPE]=type;
  t->p[P_E0]=3;t->p[P_E2]=80;t->p[P_E3]=127;t->p[P_E5]=40;t->p[P_E6]=127;t->p[P_E7]=127;t->p[P_GA]=t->p[P_GB]=127;
  t->p[P_LEVEL]=100;t->p[P_MUTE]=0;t->p[P_ATK]=t->p[P_DEC]=t->p[P_REL]=0;t->p[P_SUS]=127;
  t->p[P_DIST]=t->p[P_ITYPE]=t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=0;t->p[P_GCUT]=0;
  t->p[P_ED_FLT]=t->p[P_LD_FLT]=t->p[P_ED_SHP]=t->p[P_LD_SHP]=t->p[P_LD_AMP]=0;
 }
 if(route==1){if(project_save(0))return 1;for(unsigned c=0;c<4;c++)trk[c].p[109]=64;if(project_load(0))return 1;}
 if(route==2){for(unsigned c=0;c<4;c++){song.sel=c;if(up_store(c,"WIN"))return 1;trk[c].p[109]=64;if(up_load(c))return 1;}}
 song.master_q12=4096;
 for(unsigned c=0;c<4;c++)if(trk[c].p[109]!=value)return 1;
 /* Every route services its pending controls before creating sounding notes. */
 int32_t out[CTL*2];mix_block(out,CTL);for(unsigned c=0;c<4;c++)trk_note_on(&trk[c],60+c,110);
 FILE*f=fopen(argv[4],"wb");if(!f)return 2;audio_stats_t stats={0};
 for(unsigned b=0;b<1000;b++){mix_block(out,CTL);if(fwrite(out,sizeof out,1,f)!=1)return 2;for(unsigned j=0;j<CTL*2;j++)audio_stats_push(&stats,out[j]);}
 if(!audio_stats_sounding(&stats,32,1))return 1;fclose(f);
 printf("WIN audible roundtrip: WIN=%u TYPE=%u route=%u peak=%.0f RMS=%.6f nonzero=%llu\n",value,type,route,stats.peak,audio_stats_rms(&stats),(unsigned long long)stats.nonzero);return 0;
}
