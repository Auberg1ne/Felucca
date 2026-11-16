/* SPDX-License-Identifier: GPL-3.0-only */
/* Test-only native adapter: real editor framing/handlers and final firmware mixer. */
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"
#include "audio_stats.h"
static void boot_sound(void){reset();song.master_q12=4096;for(unsigned c=0;c<4;c++){track_t*t=&trk[c];set_engine_of(t,ENGI_GRAIN);t->engine=ENGI_GRAIN;t->p[P_E0]=3;t->p[P_E2]=80;t->p[P_E3]=127;t->p[P_E5]=40;t->p[P_E6]=127;t->p[P_E7]=127;t->p[P_GCUT]=0;t->p[P_ATK]=t->p[P_DEC]=t->p[P_REL]=0;t->p[P_SUS]=127;t->p[P_DIST]=t->p[P_ITYPE]=t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=0;}}
int main(void){boot_sound();char line[8192];while(fgets(line,sizeof line,stdin)){
 if(line[0]=='F'){uint8_t wire[4096];unsigned n=0;char*p=line+1;int off;unsigned byte;while(sscanf(p," %u%n",&byte,&off)==1){if(n>=sizeof wire||byte>255)return 2;wire[n++]=byte;p+=off;}if(n<6)return 2;request(wire[4],wire+5,n-6);printf("F");for(unsigned j=0;j<host_wire_n;j++)printf(" %u",host_wire[j]);puts("");}
 else if(line[0]=='R'){boot_sound();puts("OK");}
 else if(line[0]=='A'){unsigned win;if(sscanf(line+1,"%u",&win)!=1||win>127)return 2; /* value already set by browser commands */
  int32_t idle[CTL*2];mix_block(idle,CTL);
  for(unsigned c=0;c<4;c++){if(trk[c].p[109]!=(int)win)return 3;trk_note_on(&trk[c],60+c,110);}audio_stats_t stats={0};uint64_t hash=1469598103934665603ull;
  for(unsigned b=0;b<2048;b++){int32_t out[CTL*2];mix_block(out,CTL);for(unsigned j=0;j<CTL*2;j++){audio_stats_push(&stats,out[j]);hash=(hash^(uint32_t)out[j])*1099511628211ull;}}
  if(!audio_stats_sounding(&stats,32,1))return 4;unsigned latched=0;for(unsigned c=0;c<4;c++)for(unsigned j=0;j<GR_NG;j++)if(gr_of(&trk[c])->g[j].owner){if(gr_of(&trk[c])->g[j].window!=128+win)return 5;latched++;}
  printf("A %llu %.0f %.9f %llu %u\n",(unsigned long long)hash,stats.peak,audio_stats_rms(&stats),(unsigned long long)stats.nonzero,latched);
 }else return 2;fflush(stdout);
 }return 0;}
