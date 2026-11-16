/* SPDX-License-Identifier: GPL-3.0-only */
#define PERSISTENCE_TEST_NO_MAIN 1
#include "persistence_test.c"
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"grain-only persistence %u: %s\n",__LINE__,#x);return 1;}}while(0)
static void grain_boot(void){reset();grain_autosave_protected=0;memset(&as,0,sizeof as);for(unsigned t=0;t<NTRK;t++){set_engine_of(&trk[t],ENGI_GRAIN);trk[t].engine=ENGI_GRAIN;}}
int main(void){
 static project_t p;static project_store_t foreign,read;static uint8_t stored[sizeof nor];
 grain_boot();project_capture(&p);p.t[2].engine=ENGI_SAMPLE;p.t[2].p[109]=0;p.t[2].p[P_E0]=13;p.sum=proj_sum(&p);REQUIRE(proj_pack(&foreign,&p));
 for(unsigned off=0;off<2;off++){
  grain_boot();REQUIRE(!st_save(OBJ_AUTOSAVE,&foreign,sizeof foreign));if(off)ui_prefs|=PREF_RESTORE_OFF;else ui_prefs&=~PREF_RESTORE_OFF;
  memcpy(stored,nor,sizeof stored);REQUIRE(!autosave_boot(1) && grain_autosave_protected);unsigned writes=erases;
  TSEL->p[P_E2]++;fm1_ms+=AS_IDLE_MS+AS_GAP_MS+1000;autosave_poll();fm1_ms+=AS_IDLE_MS+AS_GAP_MS+1000;autosave_poll();
  REQUIRE(erases==writes && !memcmp(stored,nor,sizeof stored));REQUIRE(st_load(OBJ_AUTOSAVE,&read,sizeof read)==sizeof read && !memcmp(&read,&foreign,sizeof read));
 }
 grain_boot();REQUIRE(!st_save(OBJ_PROJECT0,&foreign,sizeof foreign));proj_slot[0]=foreign;memcpy(stored,nor,sizeof stored);
 REQUIRE(project_load(0) && project_save(0) && project_rename(0,"NO") && project_erase(0));REQUIRE(!memcmp(stored,nor,sizeof stored));
 /* Existing raw non-GRAIN records survive boot and updates to another slot in their bank. */
 up_bank[0].magic=UP_BANK_MAGIC;up_bank[0].rsize=sizeof(up_rec_t);up_bank[0].nslot=UP_PER_BANK;
 mig_rec(&up_bank[0],0,ENGI_SAMPLE,0,'L');up_set_value(up_rec(0),P_E0,4);static up_rec_t legacy;legacy=*up_rec(0);
 REQUIRE(!st_save(OBJ_UPRESET0,&up_bank[0],sizeof up_bank[0]));up_boot();REQUIRE(!memcmp(&legacy,up_rec(0),sizeof legacy));
 memcpy(stored,nor,sizeof stored);REQUIRE(up_load(0) && up_put(0,0) && up_rename(0,"NO"));REQUIRE(!memcmp(stored,nor,sizeof stored));
 REQUIRE(up_store(1,"GRAIN")==0);REQUIRE(!memcmp(&legacy,up_rec(0),sizeof legacy));up_boot();REQUIRE(!memcmp(&legacy,up_rec(0),sizeof legacy));
 /* Seed a legacy interrupted FM6 transaction using its retained backend. Boot must not finish/erase it. */
 grain_boot();REQUIRE(!fvb_begin() && !fvb_write(0,FM6_FACTORY[0]));const uint8_t name[10]="LEGACY";
 REQUIRE(fvb_end(name,st_crc32(FM6_FACTORY[0],FM6_PACKED))==4 && grain_autosave_protected);memcpy(stored,nor,sizeof stored);
 fvb_boot();REQUIRE(!memcmp(stored,nor,sizeof stored));REQUIRE(!autosave_boot(1) && grain_autosave_protected);
 TSEL->p[P_E2]++;fm1_ms+=AS_IDLE_MS+AS_GAP_MS+1000;autosave_poll();REQUIRE(!memcmp(stored,nor,sizeof stored));
 printf("GRAIN-only persistence: %u checks passed; physical NOR unchanged on rejection, legacy boot and staging protected\n",checks);return 0;
}
