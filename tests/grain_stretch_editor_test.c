/* SPDX-License-Identifier: GPL-3.0-only */
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"stretch editor %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
 const int values[]={0,1,64,127};uint8_t a[4]={0,108,0,0};
 reset();set_engine_of(TSEL,ENGI_GRAIN);TSEL->engine=ENGI_GRAIN;
 request(ED_DESC,a,2);
 REQUIRE(host_wire[5]==0 && host_wire[6]==108 && host_wire[7]==F_INT && ed_rv(host_wire+8)==0 && ed_rv(host_wire+10)==127 && ed_rv(host_wire+12)==0);
 REQUIRE(!strcmp((char*)host_wire+14,"STRETCH"));
 for(unsigned n=0;n<NELEM(values);n++) {
  unsigned v=values[n]+8192;a[2]=v&127;a[3]=v>>7;
  request(ED_SET,a,4);REQUIRE(TSEL->p[108]==values[n] && ed_rv(host_wire+7)==values[n]);
  request(ED_GET,a,2);REQUIRE(ed_rv(host_wire+7)==values[n]);
  trk[1].eng_req=ENGI_GRAIN;uint8_t b[4]={1,108,a[2],a[3]};request(ED_TRACK_PARAM,b,4);
  REQUIRE(trk[1].p[108]==values[n] && ed_rv(host_wire+7)==values[n]);
 }
 for(unsigned e=0;e<NENGINES;e++)if(e!=ENGI_GRAIN) {
  set_engine_of(TSEL,e);TSEL->p[108]=0;request(ED_DESC,a,2);REQUIRE(ed_rv(host_wire+8)==0 && ed_rv(host_wire+10)==0 && ed_rv(host_wire+12)==0 && !strcmp((char*)host_wire+14,"G-F RSV1"));
  a[2]=127;a[3]=64;request(ED_SET,a,4);REQUIRE(TSEL->p[108]==0 && ed_rv(host_wire+7)==0);
 }
 printf("GRAIN STRETCH editor: %u wire metadata/GET/SET checks passed\n",checks);return 0;
}
