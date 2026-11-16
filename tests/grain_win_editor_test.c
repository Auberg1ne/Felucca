/* SPDX-License-Identifier: GPL-3.0-only */
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"WIN editor %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){reset();for(unsigned t=0;t<NTRK;t++){set_engine_of(&trk[t],ENGI_GRAIN);trk[t].engine=ENGI_GRAIN;CHECK(trk[t].p[109]==64);}const unsigned vals[]={0,1,63,64,65,126,127};
 for(unsigned t=0;t<4;t++)for(unsigned i=0;i<7;i++){song.sel=t;uint8_t a[]={0,109,(vals[i]+8192)&127,(vals[i]+8192)>>7};CHECK(request(ED_SET,a,4)>5 && trk[t].p[109]==vals[i]);CHECK(request(ED_GET,a,2)>5 && trk[t].p[109]==vals[i] && ed_rv(host_wire+7)==(int)vals[i]);CHECK(!motion.count);}
 uint8_t desc[]={0,109};CHECK(request(ED_DESC,desc,2)>5);int found=0;for(unsigned i=0;i+2<host_wire_n;i++)found|=!memcmp(host_wire+i,"WIN",3);CHECK(found);CHECK(ed_rv(host_wire+8)==0 && ed_rv(host_wire+10)==127 && ed_rv(host_wire+12)==64);
 for(unsigned t=0;t<4;t++){uint32_t code=70+t|(t<<7)|(ml_code(109)<<9);ml_put(t,code);int16_t before=trk[t].p[109];CHECK(midi_learned(70+t,0));CHECK(trk[t].p[109]==before && ml_get(t)==code);CHECK(ml_learn(70+t,t,109)==2 && ml_get(t)==code);}
 uint8_t learn[]={0,70,0,109};CHECK(request(ED_LEARN_SET,learn,4)>5 && host_wire[5]!=0);CHECK(request(ED_LEARN_GET,0,0)>5);for(unsigned i=0;i<4;i++)CHECK(host_wire[6+4*i]==0);CHECK(!motion_param(109));
 printf("WIN editor: %u checks passed; unchanged framing, canonical GET/SET/DESC, quarantined learn, no motion\n",checks);return 0;}
