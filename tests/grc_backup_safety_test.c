/* SPDX-License-Identifier: GPL-3.0-only */
#define BRIDGE_TEST_NO_MAIN 1
#include "project_downgrade_bridge_test.c"
static unsigned bank_restore(unsigned b,const void*p,unsigned n){memcpy(ED_BK_RAW,p,n);ed_bk_id=6+b;ed_bk_len=ed_bk_pos=n;ed_bk_crc=st_crc32(p,n);return ed_bk_commit();}
int main(void){
 static up_bank_t legacy,marked;static project_store_t old,explicit_c;static project_t q;
 fixture("tests/fixtures/grain-win-i1/proposed-func.bin",&old);
 for(unsigned bank=0;bank<2;bank++)for(unsigned side=0;side<2;side++){
  boot();unsigned slot=bank*16;CHECK(!up_store(slot,"BASE"));legacy=up_bank[bank];legacy.r[0].ver=6;legacy.r[0].np=118;CHECK(up_bank_known(&legacy));
  if(!side){TSEL->p[118]=100;CHECK(!up_store(slot,"C100"));}else{TSEL->p[118]=0;CHECK(!up_store(slot,"C0"));TSEL->p[118]=100;CHECK(!up_store(slot,"C100"));}
  TSEL->p[118]=0;CHECK(!up_store(slot,"LATEST0")); /* inactive copy still has C */
  memset(&up_bank[bank],0,sizeof up_bank[bank]);freeze();unsigned e=erases,p=bridge_programs;
  CHECK(!up_legacy_c_ok(slot));CHECK(!up_legacy_c_ok(slot+1));
  CHECK(bank_restore(bank,&legacy,sizeof legacy)!=0);unchanged(e,p);CHECK(bank_restore(bank,&legacy,0)!=0);unchanged(e,p);
  up_boot();marked=up_bank[bank];up_set_value(&marked.r[0],118,55);CHECK(bank_restore(bank,&marked,sizeof marked)==0);CHECK(up_value(up_rec(slot),118)==55);
  CHECK(bank_restore(bank,&marked,0)!=0);up_set_value(&marked.r[0],118,0);CHECK(!bank_restore(bank,&marked,sizeof marked));CHECK(!bank_restore(bank,&marked,sizeof marked));CHECK(!bank_restore(bank,&marked,0));CHECK(up_legacy_c_ok(slot));
 }
 /* Export must preserve real C even after the retained bank cache is lost. */
 for(unsigned bank=0;bank<2;bank++){
  boot();TSEL->p[118]=100;CHECK(!up_store(bank*16,"RAW100"));marked=up_bank[bank];
  memset(&up_bank[bank],0,sizeof up_bank[bank]);freeze();unsigned e=erases,p=bridge_programs,len=0;
  const uint8_t *raw=ed_bk_object(6+bank,&len);CHECK(raw && len==sizeof marked && !memcmp(raw,&marked,len));unchanged(e,p);
  bridge_read_fail=1;len=999;CHECK(!ed_bk_object(6+bank,&len) && len==0);bridge_read_fail=0;unchanged(e,p);
 }
 for(unsigned slot=0;slot<4;slot++){
  boot();for(unsigned t=0;t<4;t++)trk[t].p[118]=(int[]){0,1,55,100}[t];CHECK(!project_save(slot));
  memset(&proj_slot[slot],0,sizeof proj_slot[slot]);freeze();unsigned e=erases,p=bridge_programs;
  CHECK(restore(slot,&old,sizeof old)!=0);unchanged(e,p);CHECK(restore(slot,&old,0)!=0);unchanged(e,p);
  project_capture(&q);for(unsigned t=0;t<4;t++)q.t[t].p[118]=55;q.sum=proj_sum(&q);CHECK(proj_pack(&explicit_c,&q));CHECK(!restore(slot,&explicit_c,sizeof explicit_c));CHECK(!project_load(slot));for(unsigned t=0;t<4;t++)CHECK(trk[t].p[118]==55);
  /* Native/current-schema C0 is explicit provenance, not silent old conversion. */
  for(unsigned t=0;t<4;t++)trk[t].p[118]=0;CHECK(!project_save(slot));CHECK(!project_save(slot));CHECK(!restore(slot,&old,0));
 }
 boot();for(unsigned t=0;t<4;t++)trk[t].p[118]=100;freeze();unsigned e=erases,p=bridge_programs;
 memcpy(ED_BK_RAW,&old,sizeof old);ed_bk_id=0;ed_bk_len=ed_bk_pos=sizeof old;ed_bk_crc=st_crc32(&old,sizeof old);CHECK(ed_bk_commit()!=0);unchanged(e,p);
 /* Both legacy restore-last and current C restore remain read-only at boot. */
 boot();CHECK(!st_save(OBJ_AUTOSAVE,&old,sizeof old));e=erases;p=bridge_programs;CHECK(autosave_boot(1));for(unsigned t=0;t<4;t++)CHECK(trk[t].p[118]==0);fm1_ms=AS_GAP_MS+AS_IDLE_MS+1000;autosave_poll();CHECK(erases==e && bridge_programs==p);
 for(unsigned t=0;t<4;t++)trk[t].p[118]=100;project_capture(&q);CHECK(proj_pack(&explicit_c,&q));CHECK(!st_save(OBJ_AUTOSAVE,&explicit_c,sizeof explicit_c));for(unsigned t=0;t<4;t++)trk[t].p[118]=0;e=erases;p=bridge_programs;CHECK(autosave_boot(1));for(unsigned t=0;t<4;t++)CHECK(trk[t].p[118]==100);CHECK(erases==e&&bridge_programs==p);
 for(unsigned cut=0;cut<14;cut++){
  boot();TSEL->p[118]=55;CHECK(!up_store(0,"OLD55"));TSEL->p[118]=100;fail_after=cut;
  CHECK(up_store(0,"NEW100")!=0);fail_after=-1;CHECK(up_value(up_rec(0),118)==55);up_boot();CHECK(up_value(up_rec(0),118)==55);
 }
 printf("GRC backup: %u checks passed; physical/stale-cache provenance, C-aware restore and restore-last\n",checks);return 0;
}
