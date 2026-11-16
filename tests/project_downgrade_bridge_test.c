/* SPDX-License-Identifier: GPL-3.0-only */
#define PERSISTENCE_TEST_NO_MAIN 1
#define BRIDGE_TEST_IO 1
#include "persistence_test.c"
/* Reuse the real backup implementation, with its existing wire helpers. */
enum { ED_BACKUP_LIST=65, ED_BACKUP_GET, ED_BACKUP_PUT };
static uint8_t ed_smp_buf[512];
static void fm1_wdt_feed(void) {}
static int ed_flash_stop(void){return transport_busy();}
static uint32_t ed_unpack7(const uint8_t*a,uint32_t n,uint8_t*out,uint32_t max){
 unsigned k=0;while(n&&k<max){unsigned mask=*a++;n--;unsigned count=n>7?7:n;if(!count || (mask>>count) || count>max-k)return 0;for(unsigned i=0;i<count;i++,n--)out[k++]=*a++|((mask>>i&1)<<7);}return n?0:k;
}
#include "../firmware/src/editor_backup.c"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"bridge line %u: %s\n",__LINE__,#x);exit(1);}}while(0)
static project_store_t valid,future;
static uint8_t frozen_nor[sizeof nor];
static project_store_t frozen_slots[4];
static track_t frozen_tracks[NTRK];
static song_t frozen_song;
static char frozen_name[sizeof proj_name];
static uint8_t frozen_cur;
static void boot(void){reset();bridge_read_fail=0;bridge_programs=0;grain_autosave_protected=0;memset(&as,0,sizeof as);for(unsigned t=0;t<NTRK;t++){set_engine_of(&trk[t],ENGI_GRAIN);trk[t].engine=ENGI_GRAIN;}}
static void fixture(const char*path,project_store_t*out){FILE*f=fopen(path,"rb");CHECK(f);CHECK(fread(out,1,sizeof *out,f)==sizeof *out && fgetc(f)==EOF);fclose(f);}
static void freeze(void){memcpy(frozen_nor,nor,sizeof nor);memcpy(frozen_slots,proj_slot,sizeof proj_slot);memcpy(frozen_tracks,trk,sizeof trk);frozen_song=song;memcpy(frozen_name,proj_name,sizeof proj_name);frozen_cur=proj_cur;}
static void unchanged(unsigned e,unsigned p){CHECK(erases==e && bridge_programs==p);CHECK(!memcmp(frozen_nor,nor,sizeof nor));CHECK(!memcmp(frozen_slots,proj_slot,sizeof proj_slot));CHECK(!memcmp(frozen_tracks,trk,sizeof trk));CHECK(!memcmp(&frozen_song,&song,sizeof song));CHECK(!memcmp(frozen_name,proj_name,sizeof proj_name) && frozen_cur==proj_cur);}
static unsigned restore(unsigned slot,const void*raw,unsigned len){memcpy(ED_BK_RAW,raw,len);ed_bk_id=slot+2;ed_bk_len=ed_bk_pos=len;ed_bk_crc=st_crc32(raw,len);return ed_bk_commit();}
static void blocked(unsigned slot,int expected){
 char name[PROJ_NAME_LEN+1];uint16_t lengths[4];uint32_t density[4];uint8_t cells[4];
 CHECK(project_slot_state(slot)==expected);CHECK(project_used(slot));CHECK(grain_slot_readonly(slot));
 CHECK(project_info(slot,name,lengths,density,cells));CHECK(!strcmp(name,expected==PS_IO?"READ ERROR":expected==PS_CORRUPT?"CORRUPT":"UNSUPPORTED"));
 unsigned e=erases,p=bridge_programs;freeze();
 CHECK(project_save(slot)!=0);unchanged(e,p);
 CHECK(project_save_as(slot,"COPY")!=0);unchanged(e,p);
 CHECK(project_rename(slot,"RENAME")!=0);unchanged(e,p);
 CHECK(project_erase(slot)!=0);unchanged(e,p);
 CHECK(project_load(slot)!=0);unchanged(e,p);
 CHECK(restore(slot,&valid,sizeof valid)!=0);unchanged(e,p);
 CHECK(restore(slot,&valid,0)!=0);unchanged(e,p);
 CHECK(ed_bk_capture()!=0);unchanged(e,p);
 ed_repn=0;CHECK(ed_backup_handle(ED_BACKUP_LIST,0,0));CHECK(ed_repn==3 && ed_rep[0]==1 && ed_rep[1]==2 && ed_rep[2]==0);unchanged(e,p);
 unsigned len=999;CHECK(!ed_bk_object(slot+2,&len) && !len);unchanged(e,p);
 proj_fetch(slot);unchanged(e,p);
 /* Lost/invalidated retained cache cannot establish NOR ownership. */
 memset(proj_slot,0,sizeof proj_slot);freeze();CHECK(project_save(slot)!=0);unchanged(e,p);CHECK(project_used(slot));
}
/* Frozen FUN10 geometry: legacy 111-parameter layout and full 128-byte FM6 patches. */
static void historical_funa(project_store_t *out)
{
 project_t q;CHECK(proj_import(&q,&valid,sizeof valid));memset(out,0,sizeof *out);
 uint32_t magic=PROJ_MAGIC_V10,size=3840,pos=68;memcpy(out->raw,&magic,4);memcpy(out->raw+4,&size,4);
 memcpy(out->raw+8,q.g,sizeof q.g);out->raw[63]=q.parts;out->raw[64]=q.phys;out->raw[66]=111;
 for(unsigned t=0;t<NTRK;t++){for(unsigned i=0;i<111;i++)out->raw[pos++]=q.t[t].p[i<103?i:P_E0+i-103]+64;out->raw[pos++]=ENGI_GRAIN;out->raw[pos++]=0;pos+=NSTEP*9;}
 chain_config_t c={0};motion_store_t m={0};memcpy(out->raw+pos,&c,sizeof c);pos+=sizeof c;memcpy(out->raw+pos,&m,sizeof m);
 memcpy(out->raw+3312,q.fm6,sizeof q.fm6);uint32_t h=proj_hash(out->raw,3836);memcpy(out->raw+3836,&h,4);
 CHECK(proj_import(&q,out,sizeof *out));
}

#ifndef BRIDGE_TEST_NO_MAIN
int main(void){
 fixture("tests/fixtures/grain-win-i1/fun11-base.bin",&valid);fixture("tests/fixtures/grain-win-i1/proposed-func.bin",&future);
 #ifdef PROJ_MAGIC_V12
 uint32_t next_magic=0x46554e44u;memcpy(future.raw,&next_magic,4);uint32_t next_sum=proj_hash(future.raw,3836);memcpy(future.raw+3836,&next_sum,4);
#endif
 for(unsigned slot=0;slot<4;slot++){
  boot();CHECK(st_save(OBJ_PROJECT0+slot,&future,sizeof future)==0);blocked(slot,PS_UNSUPPORTED);
  CHECK(project_save((slot+1)%4)==0);CHECK(project_slot_state(slot)==PS_UNSUPPORTED);CHECK(!memcmp(nor+st_sector(OBJ_PROJECT0+slot,0),frozen_nor+st_sector(OBJ_PROJECT0+slot,0),2u*ST_SECTOR));
 }
 for(unsigned i=0;i<7;i++){
  boot();project_store_t bad=future;unsigned len=sizeof bad;
  if(i==0){uint32_t m=0xdeadbeefu;memcpy(bad.raw,&m,4);}
  if(i==1){bad=valid;bad.raw[300]^=1;}
  if(i==2)len=3839;
  if(i==3)len=3;
  CHECK(st_save(OBJ_PROJECT0,&bad,len)==0);
  if(i==4)nor[st_sector(OBJ_PROJECT0,0)]^=1; /* bad storage commit */
  if(i==5)nor[st_sector(OBJ_PROJECT0,0)+ST_PAYLOAD_OFF+20]^=1; /* bad storage CRC */
  if(i==6)bridge_read_fail=1;
  blocked(0,i==6?PS_IO:i==1||i==4||i==5?PS_CORRUPT:PS_UNSUPPORTED);
 }
 boot();nor[st_sector(OBJ_PROJECT0,1)+300]=0;blocked(0,PS_CORRUPT); /* no valid header, nonempty payload */
 for(unsigned reverse=0;reverse<2;reverse++){
  boot();CHECK(!st_save(OBJ_PROJECT0,reverse?&future:&valid,sizeof valid));CHECK(!st_save(OBJ_PROJECT0,reverse?&valid:&future,sizeof valid));blocked(0,PS_UNSUPPORTED);
 }
 /* Additional opaque magic values and recognized-but-truncated objects. */
 for(unsigned i=0;i<4;i++){boot();project_store_t raw=future;uint32_t magic=(uint32_t[]){0u,UINT32_MAX,0x46554effu,PROJ_MAGIC_V11}[i];memcpy(raw.raw,&magic,4);CHECK(!st_save(OBJ_PROJECT0,&raw,i==3?3839:sizeof raw));blocked(0,i==3?PS_CORRUPT:PS_UNSUPPORTED);}
 /* Valid-looking tombstones cannot hide unowned bytes outside their length. */
 for(unsigned i=0;i<3;i++){boot();CHECK(!st_save(OBJ_PROJECT0,&valid,0));unsigned base=st_sector(OBJ_PROJECT0,0);if(i<2)nor[base+(i?300:40)]=0;else{st_hdr_t*h=(st_hdr_t*)(nor+base);h->rsv[0]=0;h->hcrc=st_crc32(h,sizeof *h-4);}blocked(0,PS_CORRUPT);}
 /* Positive physical emptiness wins over stale/random retained headers. */
 boot();proj_slot[0]=future;CHECK(project_slot_state(0)==PS_EMPTY);CHECK(project_save(0)==0);
 /* Clean erased pairs, valid historical images and owned tombstones writable. */
 boot();for(unsigned slot=0;slot<4;slot++){CHECK(project_slot_state(slot)==PS_EMPTY);CHECK(!project_used(slot));CHECK(!project_save(slot));CHECK(project_load(slot)==0);CHECK(project_rename(slot,"OK")==0);CHECK(project_erase(slot)==0);CHECK(project_slot_state(slot)==PS_EMPTY);CHECK(project_save_as(slot,"NEW")==0);}
 boot();CHECK(!st_save(OBJ_PROJECT0,&valid,sizeof valid));proj_fetch(0);CHECK(project_load(0)==0);CHECK(project_save(0)==0);CHECK(restore(1,&valid,sizeof valid)==0);
 /* Truncated/corrupt/unknown incoming replacement is rejected atomically. */
 for(unsigned len=0;len<4;len++){unsigned sizes[]={3840,3839,3,0};boot();freeze();unsigned e=erases,p=bridge_programs;CHECK(restore(0,&future,sizes[len])!=0 || sizes[len]==0);if(sizes[len])unchanged(e,p);}
 /* Restore-last ignores foreign data, and idle changes cannot overwrite it. */
 boot();CHECK(!st_save(OBJ_AUTOSAVE,&future,sizeof future));freeze();unsigned e=erases,p=bridge_programs;CHECK(!autosave_boot(1));CHECK(grain_autosave_protected);unchanged(e,p);trk[0].p[P_E1]++;fm1_ms=AS_IDLE_MS+AS_GAP_MS+1000;freeze();autosave_poll();unchanged(e,p);CHECK(fvb_begin()!=0);unchanged(e,p);
 /* Refetch after a simulated reset must discover physical ownership afresh. */
 boot();CHECK(!st_save(OBJ_PROJECT0+3,&future,sizeof future));memcpy(frozen_nor,nor,sizeof nor);boot();memcpy(nor,frozen_nor,sizeof nor);blocked(3,PS_UNSUPPORTED);
 /* Historical FUNA remains readable/writable with exactly the old geometry. */
 boot();project_store_t funa;historical_funa(&funa);CHECK(!st_save(OBJ_PROJECT0,&funa,sizeof funa));proj_fetch(0);CHECK(project_slot_state(0)==PS_VALID);CHECK(!project_load(0));CHECK(!project_save(0));
 /* A checksum-valid known header with an invalid step count is not a
  * verified fallback. FUNA patch high bits instead have a documented mask. */
 boot();historical_funa(&funa);funa.raw[68+111+2+4]=7;uint32_t corrupt_sum=proj_hash(funa.raw,3836);memcpy(funa.raw+3836,&corrupt_sum,4);CHECK(!proj_import(&proj_scratch,&funa,sizeof funa));CHECK(!st_save(OBJ_AUTOSAVE,&funa,sizeof funa));freeze();unsigned bad_erases=erases,bad_programs=bridge_programs;CHECK(!autosave_boot(1));CHECK(grain_autosave_protected);unchanged(bad_erases,bad_programs);
 /* A decoded non-GRAIN object is distinct from unknown physical ownership. */
 boot();project_t legacy;CHECK(proj_import(&legacy,&valid,sizeof valid));legacy.t[0].engine=ENGI_FM6;legacy.t[0].p[109]=0;project_store_t foreign;CHECK(proj_pack(&foreign,&legacy));CHECK(!st_save(OBJ_PROJECT0,&foreign,sizeof foreign));CHECK(project_slot_state(0)==(FELUCCA_GRAIN_ONLY?PS_INCOMPATIBLE:PS_VALID));CHECK(grain_slot_readonly(0)==!!FELUCCA_GRAIN_ONLY);
 /* Runtime restore/init cannot grant permission over an unrelated protected slot. */
 boot();CHECK(!st_save(OBJ_PROJECT0,&future,sizeof future));project_t current;CHECK(proj_import(&current,&valid,sizeof valid));CHECK(!project_restore_runtime(&current));apply_preset_to(&trk[0],0);freeze();unsigned er=erases,pr=bridge_programs;CHECK(project_save_as(0,"INIT COPY")!=0);unchanged(er,pr);
 /* Healthy autosaves continue across both copies without a false ownership latch. */
 boot();CHECK(!autosave_boot(1));
 for(unsigned n=0;n<4;n++){trk[0].p[P_E1]++;for(unsigned ms=0;ms<AS_IDLE_MS+AS_GAP_MS+2000u;ms+=1000u){fm1_ms+=1000;autosave_poll();}CHECK(!grain_autosave_protected && as.writes>=n+1u);}
 CHECK(st_load(OBJ_AUTOSAVE,&funa,sizeof funa)==sizeof funa);CHECK(proj_import(&legacy,&funa,sizeof funa));CHECK(legacy.t[0].p[P_E1]==trk[0].p[P_E1]);
 /* A complete known FM6 stage is not an unsupported future project. */
 for(unsigned unknown=0;unknown<2;unknown++){
  boot();memset(st_buf,0,FVB_LEN);fvb_info_t*info=(fvb_info_t*)st_buf;info->magic=FVB_MAGIC;info->ver=1;info->n=FM6_NBANK;
  unsigned base=st_sector(OBJ_AUTOSAVE,0);CHECK(!fvb_prog(base+FVB_INFO,st_buf,FVB_LEN));CHECK(!fvb_commit(base,1,1));
  if(unknown)CHECK(!st_save_to(OBJ_AUTOSAVE,&future,sizeof future,1));
  CHECK(project_autosave_writable()==(!unknown && !FELUCCA_GRAIN_ONLY));
  if(unknown || FELUCCA_GRAIN_ONLY){freeze();unsigned e=erases,p=bridge_programs;CHECK(fvb_begin()!=0);unchanged(e,p);}else CHECK(!fvb_begin());
 }
 /* Export a valid physical project after cache loss without altering cache
  * or the independently captured live-project backup. */
 boot();CHECK(!st_save(OBJ_PROJECT0,&valid,sizeof valid));trk[0].p[P_E1]=91;CHECK(!ed_bk_capture());unsigned active_crc=st_crc32(ED_BK_RAW,sizeof(project_store_t));freeze();unsigned before_erases=erases,before_programs=bridge_programs;unsigned exported_len=0;const uint8_t*exported=ed_bk_object(2,&exported_len);CHECK(exported && exported_len==3840);CHECK(proj_import(&legacy,exported,exported_len));CHECK(legacy.t[0].engine==ENGI_GRAIN && legacy.t[0].p[P_E1]!=91);CHECK(st_crc32(ED_BK_RAW,sizeof(project_store_t))==active_crc);unchanged(before_erases,before_programs);
 ed_repn=0;CHECK(ed_backup_handle(ED_BACKUP_LIST,0,0));CHECK(ed_rep[0]==1 && ed_rep[1]==0 && ed_rep[2]==ED_BK_N);unchanged(before_erases,before_programs);
 /* Known valid A/B history remains writable through repeated saves. */
 boot();for(unsigned k=0;k<5;k++)CHECK(!project_save(0));

 CHECK(P_COUNT==118 && P_E0==110 && sizeof(project_store_t)==3840 && sizeof(up_rec_t)==192);
 printf("project downgrade bridge: %u checks passed; physical NOR/retained atomicity, all slot writers and backup guards\n",checks);return 0;
}

#endif
