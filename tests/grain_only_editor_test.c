/* SPDX-License-Identifier: GPL-3.0-only */
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"grain-only editor %u: %s\n",__LINE__,#x);return 1;}}while(0)
static void boot_grain(void){reset();grain_autosave_protected=0;for(unsigned t=0;t<NTRK;t++){set_engine_of(&trk[t],ENGI_GRAIN);trk[t].engine=ENGI_GRAIN;}}
int main(void){
 boot_grain();request(ED_INFO,0,0);unsigned meta=6u+(unsigned)strlen((char*)host_wire+5);REQUIRE(host_wire[meta]==NENGINES && host_wire[meta+1]==P_COUNT);
 int found=0;for(unsigned i=0;i+2<host_wire_n;i++)found|=host_wire[i]==0x47 && host_wire[i+1]==1 && host_wire[i+2]==ENGI_GRAIN;REQUIRE(found);
 for(unsigned e=0;e<NENGINES;e++)if(e!=ENGI_GRAIN){
  int16_t before[P_COUNT];memcpy(before,TSEL->p,sizeof before);uint8_t a[]={1,G_ENGSEL,(e+8192)&127,(e+8192)>>7};
  REQUIRE(!request(ED_SET,a,4));REQUIRE(TSEL->eng_req==ENGI_GRAIN && !memcmp(before,TSEL->p,sizeof before));
  uint8_t b[]={e,0};REQUIRE(!request(ED_PRESET,b,2));REQUIRE(TSEL->eng_req==ENGI_GRAIN && !memcmp(before,TSEL->p,sizeof before));
 }
 uint8_t a[]={0,P_GFILTER_RSVD1,64,64};request(ED_SET,a,4);REQUIRE(TSEL->p[108]==64);
 static project_t p;static project_store_t foreign,read;project_capture(&p);p.t[1].engine=ENGI_SAMPLE;p.t[1].p[P_GWIN]=0;p.t[1].p[P_E0]=13;p.sum=proj_sum(&p);REQUIRE(proj_pack(&foreign,&p));
 proj_slot[0]=foreign;uint8_t load[]={0,0};request(ED_PROJECT,load,2);REQUIRE(trk[1].eng_req==ENGI_GRAIN && !memcmp(&foreign,&proj_slot[0],sizeof foreign));
 boot_grain();flash_ok=1;unsigned rc=up_store(0,"KEEP");REQUIRE(rc==0 || rc==3);up_rec(0)->engine=ENGI_SAMPLE;up_set_value(up_rec(0),109,0);
 static up_bank_t bank;bank=up_bank[0];up_bank_check(0,sizeof bank);REQUIRE(!memcmp(&bank,&up_bank[0],sizeof bank));
 unsigned writes=host_writes;REQUIRE(up_load(0) && up_put(0,0) && up_rename(0,"NO"));REQUIRE(host_writes==writes && !memcmp(&bank,&up_bank[0],sizeof bank));
 /* Backend backup commit rejects incompatible content before any persistent write. */
 memcpy(ED_BK_RAW,&foreign,sizeof foreign);ed_bk_len=ed_bk_pos=sizeof foreign;ed_bk_crc=st_crc32(ED_BK_RAW,ed_bk_len);ed_bk_id=0;
 REQUIRE(ed_bk_commit()==2 && host_writes==writes);
 ed_bk_id=2;REQUIRE(ed_bk_commit()==2 && host_writes==writes);
 memcpy(ED_BK_RAW,&bank,sizeof bank);ed_bk_len=ed_bk_pos=sizeof bank;ed_bk_crc=st_crc32(ED_BK_RAW,ed_bk_len);ed_bk_id=6;
 REQUIRE(ed_bk_commit()==2 && host_writes==writes && !memcmp(&bank,&up_bank[0],sizeof bank));
 REQUIRE(!ed_bk_capture());uint32_t n;const uint8_t*raw=ed_bk_object(6,&n);REQUIRE(n==sizeof bank && !memcmp(raw,&bank,n));
 /* Target preflight uses shared scratch; it must not replace the incoming compatible backup. */
 boot_grain();REQUIRE(!project_save(1));project_capture(&p);p.t[0].p[108]=64;p.sum=proj_sum(&p);
 REQUIRE(proj_pack((project_store_t*)ED_BK_RAW,&p));ed_bk_len=ed_bk_pos=sizeof(project_store_t);ed_bk_crc=st_crc32(ED_BK_RAW,ed_bk_len);ed_bk_id=3;
 REQUIRE(!ed_bk_commit());REQUIRE(!project_load(1) && TSEL->p[108]==64);
 printf("GRAIN-only editor/storage: %u checks passed; protocol, read-only projects/presets/backups\n",checks);return 0;
}
