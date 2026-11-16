/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"retained %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 for(unsigned k=0;k<4;k++)for(unsigned magic=0;magic<3;magic++){
  ui_power_on();FILE*f=fopen("tests/fixtures/grain-win-i1/proposed-func.bin","rb");CHECK(f);CHECK(fread(&proj_slot[k],1,3840,f)==3840);fclose(f);
  #ifdef PROJ_MAGIC_V12
  uint32_t next_magic=0x46554e44u;memcpy(proj_slot[k].raw,&next_magic,4);uint32_t sum=proj_hash(proj_slot[k].raw,3836);memcpy(proj_slot[k].raw+3836,&sum,4);
#endif
  if(magic==1)memset(proj_slot[k].raw,0,4);if(magic==2)memset(proj_slot[k].raw,255,4);
  project_store_t copy=proj_slot[k];CHECK(project_used(k) && grain_slot_readonly(k));CHECK(project_save(k)!=0 && project_save_as(k,"COPY")!=0 && project_erase(k)!=0 && project_rename(k,"NEW")!=0 && project_load(k)!=0);CHECK(!memcmp(&copy,&proj_slot[k],sizeof copy));
 }
 ui_power_on();CHECK(!project_used(0) && project_save(0)==0 && project_load(0)==0);
 CHECK(P_COUNT==118 && P_E0==110 && param_desc_of(ENGI_GRAIN,P_GFILTER_RSVD2)->max==127);
 printf("retained project bridge: %u checks passed; RAM-only nonempty preservation, empty slots writable, WIN headless\n",checks);return 0;
}
