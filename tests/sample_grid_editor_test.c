/* SPDX-License-Identifier: GPL-3.0-only */
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"SAMPLE editor %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 const unsigned ids[]={110,111,116,38,41,42,43,39,40,112,113,114,37,44,95,115,117};
 reset();set_engine_of(TSEL,ENGI_SAMPLE);TSEL->engine=ENGI_SAMPLE;
 set_engine_of(&trk[1],ENGI_SAMPLE);trk[1].engine=ENGI_SAMPLE;
 for(unsigned i=0;i<NELEM(ids);i++){
  unsigned id=ids[i];const param_desc_t*d=param_desc_of(ENGI_SAMPLE,id);uint8_t a[]={0,id,0,0};
  request(ED_DESC,a,2);REQUIRE(host_wire[6]==id && ed_rv(host_wire+8)==d->min && ed_rv(host_wire+10)==d->max && ed_rv(host_wire+12)==d->def);
  for(unsigned end=0;end<2;end++){
   int value=end?d->max:d->min; /* Source aliases are tested by the existing factory/format suites. */
   unsigned wire=value+8192;a[2]=wire&127;a[3]=wire>>7;request(ED_SET,a,4);REQUIRE(TSEL->p[id]==value);
   request(ED_GET,a,2);REQUIRE(ed_rv(host_wire+7)==value);
   uint8_t b[]={1,id,a[2],a[3]};request(ED_TRACK_PARAM,b,4);REQUIRE(trk[1].p[id]==value);
  }
 }
 REQUIRE(P_COUNT==118 && P_E0==110 && sizeof(project_store_t)==3840);
 printf("SAMPLE editor: %u checks passed; all hidden and visible IDs retain wire access\n",checks);return 0;
}
