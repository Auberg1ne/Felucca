/* SPDX-License-Identifier: GPL-3.0-only */
/* Compact current-format snapshots; historical binaries are frozen separately. */
#define PERSISTENCE_TEST_NO_MAIN 1
#include "persistence_test.c"
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"quality storage %u: %s\n",__LINE__,#x);return 1;}}while(0)
static uint64_t bytes(const void*p,unsigned n){const unsigned char*b=p;uint64_t h=UINT64_C(14695981039346656037);for(unsigned i=0;i<n;i++){h^=b[i];h*=UINT64_C(1099511628211);}return h;}
int main(void){
 REQUIRE(P_COUNT==118&&P_E0==110&&sizeof(project_store_t)==3840&&sizeof(up_rec_t)==192&&UP_VER==6&&UP_VER_GRID==7);
 /* Historical common-before-engine mapping, and const input preservation. */
 for(unsigned np=8;np<=118;np++){
  int16_t input[118],def[118],out[118];for(unsigned j=0;j<118;j++){input[j]=j;def[j]=-1;}
  uint64_t before=bytes(input,sizeof input);params_by_count(out,input,np,def);
  for(unsigned j=0;j<110;j++)REQUIRE(out[j]==(j<np-8?(int)j:-1));
  for(unsigned j=0;j<8;j++)REQUIRE(out[110+j]==(int)(np-8+j));REQUIRE(before==bytes(input,sizeof input));
 }
 printf("{\"project_bytes\":3840,\"preset_bytes\":192,\"pcount\":118,\"pe0\":110,\"project_magic\":%u,\"preset_versions\":[6,7],\"snapshots\":[",PROJ_MAGIC_V12);
 for(unsigned kind=0;kind<4;kind++){
  reset();for(unsigned t=0;t<4;t++){set_engine_of(&trk[t],ENGI_GRAIN);trk[t].engine=ENGI_GRAIN;trk[t].p[P_GTYPE]=kind;trk[t].p[P_ED_FX]=17+t;trk[t].p[109]=64;}
  static project_t p,q;static project_store_t store;project_capture(&p);REQUIRE(proj_pack(&store,&p));
  uint64_t hash=bytes(&store,sizeof store);REQUIRE(proj_import(&q,&store,sizeof store));REQUIRE(hash==bytes(&store,sizeof store));
  for(unsigned t=0;t<4;t++)REQUIRE(q.t[t].p[P_GTYPE]==kind&&q.t[t].p[P_ED_FX]==17+(int)t&&q.t[t].p[109]==64);
  REQUIRE(!up_store(0,"BASELINE"));const up_rec_t*r=up_rec(0);REQUIRE(up_valid(r)&&r->ver==6&&r->np==118);
  printf("%s{\"type\":%u,\"project_fnv64\":\"%016llx\",\"preset_fnv64\":\"%016llx\"}",kind?",":"",kind,(unsigned long long)hash,(unsigned long long)bytes(r,sizeof *r));
 }
 puts("]}");return 0;
}
