/* SPDX-License-Identifier: GPL-3.0-only */
/* Compile exclusively against the exact B1 reader and physical NOR fixture.
 * Inputs are actual payloads emitted by grain_win_persistence_test, not hand-built future headers. */
#ifndef WIN_B1_PERSISTENCE_SOURCE
#error "Set WIN_B1_PERSISTENCE_SOURCE to exact B1 tests/persistence_test.c"
#endif
#define PERSISTENCE_TEST_NO_MAIN 1
#define BRIDGE_TEST_IO 1
#include WIN_B1_PERSISTENCE_SOURCE
enum { ED_BACKUP_LIST=65, ED_BACKUP_GET, ED_BACKUP_PUT };
static uint8_t ed_smp_buf[512];
static void fm1_wdt_feed(void) {}
static int ed_flash_stop(void){return transport_busy();}
static uint32_t ed_unpack7(const uint8_t*a,uint32_t n,uint8_t*out,uint32_t max){unsigned k=0;while(n&&k<max){unsigned mask=*a++;n--;unsigned count=n>7?7:n;if(!count||(mask>>count)||count>max-k)return 0;for(unsigned i=0;i<count;i++,n--)out[k++]=*a++|((mask>>i&1)<<7);}return n?0:k;}
#ifndef WIN_B1_BACKUP_SOURCE
#error "Set WIN_B1_BACKUP_SOURCE to exact B1 firmware/src/editor_backup.c"
#endif
#include WIN_B1_BACKUP_SOURCE
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"B1 oracle %u: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint8_t before[sizeof nor];static project_store_t slots[4];static track_t tracks[4];static song_t oldsong;
static void boot(void){reset();bridge_programs=0;bridge_read_fail=0;grain_autosave_protected=0;memset(&as,0,sizeof as);for(unsigned t=0;t<4;t++){set_engine_of(&trk[t],ENGI_GRAIN);trk[t].engine=ENGI_GRAIN;}}
static void freeze(void){memcpy(before,nor,sizeof nor);memcpy(slots,proj_slot,sizeof slots);memcpy(tracks,trk,sizeof tracks);oldsong=song;}
static void same(unsigned e,unsigned p){CHECK(erases==e && bridge_programs==p);CHECK(!memcmp(before,nor,sizeof nor));CHECK(!memcmp(slots,proj_slot,sizeof slots));CHECK(!memcmp(tracks,trk,sizeof tracks));CHECK(!memcmp(&oldsong,&song,sizeof song));}
static void read_payload(const char*dir,const char*name,void*p,unsigned n){char path[512];snprintf(path,sizeof path,"%s/%s",dir,name);FILE*f=fopen(path,"rb");CHECK(f && fread(p,1,n,f)==n && fgetc(f)==EOF);fclose(f);}
static unsigned restore(unsigned id,const void*p,unsigned n){memcpy(ED_BK_RAW,p,n);ed_bk_id=id;ed_bk_len=ed_bk_pos=n;ed_bk_crc=st_crc32(p,n);return ed_bk_commit();}
int main(int argc,char**argv){CHECK(argc==2);const unsigned values[]={0,1,63,64,65,126,127};static project_store_t payload,known;static project_t q;
 boot();project_capture(&q);CHECK(proj_pack(&known,&q));
 for(unsigned v=0;v<7;v++)for(unsigned slot=0;slot<4;slot++){
  char name[40];snprintf(name,sizeof name,"func-%u.bin",values[v]);read_payload(argv[1],name,&payload,sizeof payload);CHECK(*(uint32_t*)payload.raw==0x46554e43);CHECK(!proj_import(&q,&payload,sizeof payload));boot();CHECK(!st_save(OBJ_PROJECT0+slot,&payload,sizeof payload));CHECK(project_slot_state(slot)==PS_UNSUPPORTED && project_used(slot));unsigned e=erases,p=bridge_programs;freeze();
  CHECK(project_save(slot)!=0);same(e,p);CHECK(project_save_as(slot,"NO")!=0);same(e,p);CHECK(project_rename(slot,"NO")!=0);same(e,p);CHECK(project_erase(slot)!=0);same(e,p);CHECK(project_load(slot)!=0);same(e,p);CHECK(restore(slot+2,&known,sizeof known)!=0);same(e,p);CHECK(restore(slot+2,&known,0)!=0);same(e,p);
  memset(proj_slot,0,sizeof proj_slot);freeze();proj_fetch(slot);same(e,p);CHECK(project_save(slot)!=0);same(e,p);
 }
 boot();CHECK(!st_save(OBJ_AUTOSAVE,&payload,sizeof payload));freeze();unsigned e=erases,p=bridge_programs;CHECK(!autosave_boot(1) && grain_autosave_protected);same(e,p);trk[0].p[P_E1]++;freeze();fm1_ms=AS_IDLE_MS+AS_GAP_MS+1000;autosave_poll();same(e,p);CHECK(fvb_begin()!=0);same(e,p);
 /* B1 preserves unsupported new user records and rejects replacing their bank. */
 static up_bank_t bank;read_payload(argv[1],"up-bank.bin",&bank,sizeof bank);CHECK(bank.r[0].ver==6 && bank.r[1].ver==7);boot();CHECK(!st_save(OBJ_UPRESET0,&bank,sizeof bank));memcpy(before,nor,sizeof nor);e=erases;p=bridge_programs;up_boot();CHECK(erases==e && bridge_programs==p && !memcmp(before,nor,sizeof nor));CHECK(!up_used(0) && up_rec(0)->used==UP_USED);up_bank_t saved=up_bank[0];CHECK(up_put(0,0)!=0 && up_rename(0,"NO")!=0 && up_store(0,"NO")!=0);CHECK(!memcmp(&saved,&up_bank[0],sizeof saved));CHECK(restore(6,&saved,sizeof saved)!=0);CHECK(restore(6,&saved,0)!=0);CHECK(erases==e && bridge_programs==p && !memcmp(before,nor,sizeof nor));
 printf("exact B1 WIN downgrade oracle: %u checks passed; real FUNC/UP6/7 payloads, raw NOR preserved\n",checks);return 0;
}
