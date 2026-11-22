/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <setjmp.h>
#define PRESET_BANK_TEST_IO 1
static int preset_bank_test_readfail(uint32_t off, uint32_t n);
static void preset_bank_test_hook(unsigned op, uint32_t off, uint32_t n);
#define BRIDGE_TEST_NO_MAIN 1
#include "project_downgrade_bridge_test.c"
static unsigned read_mask, events, cut_at, erase_attempts, fail_cleanup;
static jmp_buf power_cut;
static int preset_bank_test_readfail(uint32_t off,uint32_t n){
 (void)n;return off>=0xdc000u && off<0xe0000u && (read_mask & (1u<<((off-0xdc000u)/4096u)));
}
static void preset_bank_test_hook(unsigned op,uint32_t off,uint32_t n){
 (void)n;if(off<0xdc000u||off>=0xe0000u)return;
 if(!op){erase_attempts++;if(fail_cleanup && erase_attempts==2)erase_error=1;return;}
 events++;if(cut_at && events==cut_at)longjmp(power_cut,1);
}
static void start(void){read_mask=events=cut_at=erase_attempts=fail_cleanup=0;boot();}
static up_bank_t known, suspect, cached[2];
static favorites_t saved_favorites;
static uint32_t saved_gen;
static void capture(void){freeze();memcpy(cached,up_bank,sizeof cached);saved_favorites=favorites;saved_gen=up_gen;}
static void intact(unsigned e,unsigned p){unchanged(e,p);CHECK(!memcmp(cached,up_bank,sizeof cached));CHECK(!memcmp(&saved_favorites,&favorites,sizeof favorites));CHECK(up_gen==saved_gen);}
/* Raw fixture creation deliberately bypasses the writer under test. */
static void seed(unsigned bank,unsigned copy,const void *data,unsigned len,unsigned seq){
 unsigned base=st_sector(OBJ_UPRESET0+bank,copy);st_hdr_t h;
 memset(nor+base,255,4096);memset(&h,255,sizeof h);h.magic=ST_MAGIC;h.type=OBJ_UPRESET0+bank;h.slot=copy;h.seq=seq;h.len=len;h.crc=st_crc32(data,len);h.hcrc=st_crc32(&h,sizeof h-4);
 memcpy(nor+base+ST_PAYLOAD_OFF,data,len);memcpy(nor+base,&h,sizeof h);
}
static unsigned bank_restore(unsigned bank,const void *data,unsigned len){
 memcpy(ED_BK_RAW,data,len);ed_bk_id=6+bank;ed_bk_len=ed_bk_pos=len;ed_bk_crc=st_crc32(data,len);return ed_bk_commit();
}
static void refuse(unsigned bank){
 unsigned slot=bank*16,e=erases,p=bridge_programs;capture();
 CHECK(up_store(slot,"NO")!=0);intact(e,p);
 CHECK(up_rename(slot,"NO")!=0);intact(e,p);
 CHECK(up_put(slot,0)!=0);intact(e,p);
 CHECK(up_put(slot,&known.r[0])!=0);intact(e,p);
 CHECK(bank_restore(bank,&known,sizeof known)!=0);intact(e,p);
 CHECK(bank_restore(bank,&known,0)!=0);intact(e,p);
 CHECK(st_save(OBJ_UPRESET0+bank,&known,sizeof known)==ST_OWNER_DENIED);intact(e,p);
 for(int to=0;to<2;to++){CHECK(st_save_to(OBJ_UPRESET0+bank,&known,0,to)==ST_OWNER_DENIED);intact(e,p);}
 memset(up_bank,0,sizeof up_bank);capture();CHECK(up_store(slot,"STALE")!=0);intact(e,p);
 CHECK(bank_restore(bank,&known,0)!=0);intact(e,p);
 up_boot();capture();CHECK(up_store(slot,"REFETCH")!=0);intact(e,p);
}
int main(void){
 start();CHECK(!up_store(0,"SEED"));known=up_bank[0];CHECK(up_bank_known(&known));
 /* Both banks, both copies, and suspect copy older/newer than readable copy. */
 for(unsigned bank=0;bank<2;bank++)for(unsigned copy=0;copy<2;copy++)for(unsigned newer=0;newer<2;newer++)for(unsigned mode=0;mode<19;mode++){
  start();suspect=known;unsigned len=sizeof suspect;
  if(mode==0)suspect.r[0].ver=10;
  if(mode==1)suspect.r[0].ver=11;
  if(mode==2)suspect.magic^=1;
  if(mode==3)suspect.rsize--;
  if(mode==4)suspect.nslot--;
  if(mode==5)len--;
  if(mode==6){suspect.r[1].name[0]='?';}
  if(mode==7)suspect.r[1].used=0xa6;
  if(mode==8){suspect.r[0].engine=0;up_set_value(&suspect.r[0],109,0);}
  seed(bank,1-copy,&known,sizeof known,2);seed(bank,copy,&suspect,len,newer?3:1);
  unsigned base=st_sector(OBJ_UPRESET0+bank,copy);st_hdr_t *h=(st_hdr_t*)(nor+base);
  if(mode==9)nor[base+ST_PAYLOAD_OFF+20]^=1;
  if(mode==10)h->hcrc^=1;
  if(mode==11)nor[base+40]=0;
  if(mode==12)nor[base+4095]=0;
  if(mode==13){h->rsv[0]=0;h->hcrc=st_crc32(h,sizeof *h-4);}
  if(mode==14){h->type=OBJ_SETTINGS;h->hcrc=st_crc32(h,sizeof *h-4);}
  if(mode==15){memset(nor+base,255,sizeof *h);}
  if(mode==16){seed(bank,copy,&known,0,newer?3:1);nor[base+300]=0;}
  if(mode==17)read_mask=1u<<(bank*2+copy);
  if(mode==18){h->len=ST_PAYLOAD_MAX+1;h->hcrc=st_crc32(h,sizeof *h-4);}
  favorite_set(NENGINES,bank*16+1,1);memcpy(frozen_nor,nor,sizeof nor);up_boot();
  CHECK(erases==0 && bridge_programs==0 && !memcmp(frozen_nor,nor,sizeof nor));
  CHECK(favorite_has(NENGINES,bank*16+1));CHECK(!up_bank_writable(bank));refuse(bank);
  read_mask=0;CHECK(!up_store((1-bank)*16,"ISOLATED"));
 }
 /* All historically accepted record versions retain normal save/recovery. */
 for(unsigned ver=1;ver<=9;ver++)for(unsigned bank=0;bank<2;bank++)for(unsigned copy=0;copy<2;copy++){
  start();suspect=known;suspect.r[0].ver=ver;if(ver==6 || ver==7)suspect.r[0].np=118;
  if(ver<=5){char path[128];snprintf(path,sizeof path,"tests/fixtures/grain-win-i1r/up-v%u-%u.bin",ver,ver>=4?127:0);FILE*f=fopen(path,"rb");CHECK(f);CHECK(fread(&suspect.r[0],1,192,f)==192);fclose(f);}
  CHECK(up_valid(&suspect.r[0]));
  seed(bank,copy,&suspect,sizeof suspect,UINT32_MAX);up_boot();CHECK(up_bank_writable(bank));
  CHECK(!up_store(bank*16+1,"WRAP"));CHECK(up_used(bank*16));CHECK(!up_rename(bank*16+1,"RENAMED"));
  CHECK(!up_put(bank*16+1,0));CHECK(!bank_restore(bank,&known,sizeof known));CHECK(!bank_restore(bank,&known,0));
  up_boot();CHECK(up_bank_writable(bank));CHECK(!up_store(bank*16,"AFTER CLEAR"));
 }
 /* Known-source invalid input may not create an unknown bank. */
 start();suspect=known;suspect.r[0].ver=10;capture();unsigned e=erases,p=bridge_programs;
 CHECK(up_put(0,&suspect.r[0])!=0);intact(e,p);CHECK(bank_restore(0,&suspect,sizeof suspect)!=0);intact(e,p);
 flash_ok=0;capture();CHECK(up_store(0,"OFFLINE")!=0);intact(e,p);flash_ok=1;
 /* Runtime INIT/CLEAN do not erase preset banks, even with protected bytes. */
 seed(0,0,&suspect,sizeof suspect,1);up_boot();memcpy(frozen_nor,nor,sizeof nor);e=erases;p=bridge_programs;
 apply_preset_to(TSEL,0);clean_engine_of(TSEL);CHECK(erases==e&&bridge_programs==p&&!memcmp(nor,frozen_nor,sizeof nor));
 /* Synchronous error rollback: transaction owns only its own incomplete target. */
 for(unsigned bank=0;bank<2;bank++)for(unsigned copy=0;copy<2;copy++)for(unsigned fail=0;fail<14;fail++){
  start();seed(bank,copy,&known,sizeof known,1);up_boot();up_bank_t before=up_bank[bank];saved_gen=up_gen;
  fail_after=fail;CHECK(up_store(bank*16,"FAIL")!=0);CHECK(!memcmp(&before,&up_bank[bank],sizeof before));CHECK(up_gen==saved_gen);
  fail_after=-1;CHECK(up_bank_writable(bank));up_boot();CHECK(up_used(bank*16));CHECK(!up_store(bank*16,"RETRY"));
 }
 /* Erase failure and backup replacement/clear failures preserve prior ownership. */
 for(unsigned bank=0;bank<2;bank++)for(unsigned copy=0;copy<2;copy++){
  start();seed(bank,copy,&known,sizeof known,1);up_boot();capture();unsigned e0=erases,p0=bridge_programs;
  erase_error=1;CHECK(up_store(bank*16,"ERASE FAIL")!=0);
  /* An attempted erase is counted, but the simulated failure changes no bytes. */
  intact(e0+1,p0);erase_error=0;
  for(unsigned clear=0;clear<2;clear++){
   up_bank_t before=up_bank[bank];fail_after=clear?0:1;CHECK(bank_restore(bank,&known,clear?0:sizeof known)!=0);
   CHECK(!memcmp(&before,&up_bank[bank],sizeof before));fail_after=-1;CHECK(up_bank_writable(bank));
   CHECK(!bank_restore(bank,&known,sizeof known));
  }
 }
 /* Failed rollback leaves protected evidence, never a grant to erase later. */
 start();seed(0,0,&known,sizeof known,1);up_boot();events=erase_attempts=0;fail_cleanup=1;fail_after=1;
 CHECK(up_store(0,"FAIL")!=0);fail_after=-1;erase_error=0;fail_cleanup=0;CHECK(!up_bank_writable(0));refuse(0);
 /* Abrupt reset after erase/every payload page/header: no cleanup stack survives. */
 for(unsigned bank=0;bank<2;bank++)for(unsigned copy=0;copy<2;copy++)for(unsigned cut=1;cut<=15;cut++){
  start();seed(bank,copy,&known,sizeof known,1);up_boot();events=0;cut_at=cut;
  if(!setjmp(power_cut)){(void)up_store(bank*16,"NEW");CHECK(0);}
  cut_at=0;CHECK(events==cut);up_boot();CHECK(up_used(bank*16));
  unsigned oldbase=st_sector(OBJ_UPRESET0+bank,copy);CHECK(!memcmp(nor+oldbase+ST_PAYLOAD_OFF,&known,sizeof known));
  if(cut>1 && cut<15){CHECK(!up_bank_writable(bank));refuse(bank);}else{CHECK(up_bank_writable(bank));CHECK(!up_store(bank*16,"RECOVER"));}
 }
 printf("Preset-bank ownership: %u checks passed; both copies/banks, faults, cleanup and abrupt reset covered\n",checks);return 0;
}
