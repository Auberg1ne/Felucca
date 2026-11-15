/* SPDX-License-Identifier: GPL-3.0-only */
/* Deliberately failing release gate against the exact pre-I1 revision.
 * No proposed production reader/conversion code is used. Compile with
 * WIN_I1_BASE_PERSISTENCE_SOURCE pointing at ca62bb1/tests/persistence_test.c.
 * Exit 1 = required downgrade preservation failed, 2 = broken probe/fixture. */
#ifndef WIN_I1_BASE_PERSISTENCE_SOURCE
#error "Choose the exact pre-I1 persistence fixture explicitly"
#endif
#define PERSISTENCE_TEST_NO_MAIN 1
#include WIN_I1_BASE_PERSISTENCE_SOURCE
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"probe setup line %u: %s\n",__LINE__,#x);return 2;}}while(0)
static int read_fixture(const char *path, project_store_t *out){
 FILE*f=fopen(path,"rb");if(!f)return 0;
 int ok=fread(out,1,sizeof *out,f)==sizeof *out && fgetc(f)==EOF;fclose(f);return ok;
}
int main(void){
 static project_t q;static project_store_t old,future,read;static uint8_t before[0x100000];
 reset();grain_autosave_protected=0;memset(&as,0,sizeof as);
 for(unsigned t=0;t<NTRK;t++){set_engine_of(&trk[t],ENGI_GRAIN);trk[t].engine=ENGI_GRAIN;}
 CHECK(read_fixture("tests/fixtures/grain-win-i1/fun11-base.bin",&old));
 CHECK(read_fixture("tests/fixtures/grain-win-i1/proposed-func.bin",&future));
 CHECK(sizeof future==3840 && P_COUNT==118 && P_E0==110 && sizeof(up_rec_t)==192);
 CHECK(proj_import(&q,&old,sizeof old));
 CHECK(*(uint32_t*)future.raw==0x46554e43u);
 CHECK(*(uint32_t*)(future.raw+3836)==proj_hash(future.raw,3836));
 CHECK(!proj_import(&q,&future,sizeof future));
 CHECK(st_save(OBJ_PROJECT0,&future,sizeof future)==0);
 memcpy(before,nor,sizeof nor);proj_slot[0]=future;
 int readonly=grain_slot_readonly(0);
 proj_fetch(0);CHECK(!memcmp(before,nor,sizeof nor));
 int load_rc=project_load(0);CHECK(load_rc!=0);
 CHECK(!memcmp(before,nor,sizeof nor));
 unsigned start_erases=erases;int save_rc=project_save(0);
 CHECK(st_load(OBJ_PROJECT0,&read,sizeof read)==sizeof read);
 int preserved=!memcmp(&read,&future,sizeof read);
 printf("pre-I1 setup/read checks: %u passed\n",checks);
 printf("unknown slot readonly=%d; load rc=%d; SAVE rc=%d; erase delta=%u; preserved=%d; replacement magic=%08x\n",
        readonly,load_rc,save_rc,erases-start_erases,preserved,*(uint32_t*)read.raw);
 if(!preserved){fprintf(stderr,"FAIL: previous revision overwrites unsupported future project through normal SAVE\n");return 1;}
 return 0;
}
