/* SPDX-License-Identifier: GPL-3.0-only */
#define BRIDGE_TEST_NO_MAIN 1
#include "project_downgrade_bridge_test.c"
static unsigned read_fixture_bytes(const char *name,void *dst,unsigned cap){char path[256];snprintf(path,sizeof path,"tests/fixtures/grain-win-i1r/%s.bin",name);FILE*f=fopen(path,"rb");CHECK(f);unsigned n=fread(dst,1,cap,f);CHECK(feof(f)||fgetc(f)==EOF);fclose(f);return n;}
static void seal_raw(project_store_t*s){uint32_t h=proj_hash(s->raw,3836);memcpy(s->raw+3836,&h,4);}
static void dump(const char*dir,const char*name,const void*p,unsigned n){if(!dir)return;char path[512];snprintf(path,sizeof path,"%s/%s",dir,name);FILE*f=fopen(path,"wb");CHECK(f && fwrite(p,1,n,f)==n);fclose(f);}
static void expect_win(const project_t*q,unsigned w){for(unsigned t=0;t<NTRK;t++)CHECK(q->t[t].p[109]==(q->t[t].engine==ENGI_GRAIN?w:0));}
int main(int argc,char**argv){
 const unsigned values[]={0,1,63,64,65,126,127};static uint8_t raw[sizeof(project_t)];static project_t p,q,old;char name[64];boot();
 CHECK(P_COUNT==118 && P_E0==110 && P_GWIN==109 && sizeof(project_store_t)==3840 && sizeof(up_rec_t)==192);
 for(unsigned e=0;e<NENGINES;e++)for(unsigned id=0;id<P_COUNT;id++){
  const param_desc_t*d=param_desc_of(e,id);
  if(e==ENGI_GRAIN && (id==108 || id==109))CHECK(d->min==0 && d->max==127 && d->def==(id==109?64:0));
  else CHECK(d==(id>=P_E0?&ENGINES[e]->edit[id-P_E0]:&TP[id]));
 }
 for(unsigned v=1;v<=10;v++){snprintf(name,sizeof name,"v%u",v);unsigned n=read_fixture_bytes(name,raw,sizeof raw);CHECK(proj_import(&q,raw,n));expect_win(&q,64);}
 for(unsigned v=0;v<NELEM(values);v++)for(unsigned retained=0;retained<2;retained++){
  snprintf(name,sizeof name,retained?"retained-%u":"v11-%u",values[v]);unsigned n=read_fixture_bytes(name,raw,sizeof raw);uint32_t h=st_crc32(raw,n);CHECK(proj_import(&q,raw,n));expect_win(&q,64);CHECK(st_crc32(raw,n)==h);CHECK(!project_restore_runtime(&q));for(unsigned t=0;t<NTRK;t++)CHECK(trk[t].p[109]==64);
  if(retained){memcpy(&old,raw,n);CHECK(!project_restore_runtime(&old));for(unsigned t=0;t<NTRK;t++)CHECK(trk[t].p[109]==64);CHECK(!memcmp(&old,raw,n));}
 }
 /* Exercise physical byte values too: every old legal biased byte, including
  * 0/1/63 (-64/-63/-1), must ignore reserved109 and become Hann. */
 for(unsigned byte=0;byte<=191;byte++){
  CHECK(read_fixture_bytes("v11-0",&valid,sizeof valid)==3840);for(unsigned t=0;t<4;t++)valid.raw[68+t*(118+2+NSTEP*9)+109]=byte;seal_raw(&valid);CHECK(proj_import(&q,&valid,sizeof valid));expect_win(&q,64);
 }
 for(unsigned t=0;t<NTRK;t++){for(unsigned pi=0;pi<ENGINES[ENGI_GRAIN]->npresets;pi++){trk[t].p[109]=0;apply_preset_to(&trk[t],pi);CHECK(trk[t].p[109]==64);}trk[t].p[109]=0;clean_engine_of(&trk[t]);CHECK(trk[t].p[109]==64);}
 boot();project_capture(&p);
 for(unsigned val=0;val<128;val++){
  for(unsigned t=0;t<NTRK;t++)p.t[t].p[109]=(val+t*31)%128;p.sum=proj_sum(&p);CHECK(proj_pack(&valid,&p));CHECK(*(uint32_t*)valid.raw==PROJ_MAGIC_V12 && valid.raw[66]==118);CHECK(proj_import(&q,&valid,sizeof valid));CHECK(!memcmp(&p,&q,sizeof p));
  CHECK(!project_restore_runtime(&q));for(unsigned t=0;t<NTRK;t++)CHECK(trk[t].p[109]==(int)(val+t*31)%128);
 }
 for(unsigned i=0;i<NELEM(values);i++){
  boot();for(unsigned t=0;t<NTRK;t++)trk[t].p[109]=values[i];project_capture(&p);CHECK(proj_pack(&valid,&p));snprintf(name,sizeof name,"func-%u.bin",values[i]);dump(argc>1?argv[1]:0,name,&valid,sizeof valid);
  CHECK(!project_save(0));CHECK(!project_save_as(1,"COPY"));CHECK(!restore(2,&valid,sizeof valid));for(unsigned t=0;t<NTRK;t++)trk[t].p[109]=64;CHECK(!project_load(1));for(unsigned t=0;t<NTRK;t++)CHECK(trk[t].p[109]==values[i]);
  for(unsigned grid=0;grid<2;grid++){
   up_rec_t r={0};r.used=UP_USED;r.ver=grid?UP_VER_GRID:UP_VER;r.engine=ENGI_GRAIN;r.np=118;memcpy(r.name,"WINDOW",6);for(unsigned j=0;j<118;j++)up_set_value(&r,j,trk[0].p[j]);CHECK(up_valid(&r));CHECK(!up_put(grid,&r));CHECK(!up_load(grid));CHECK(TSEL->p[109]==values[i]);CHECK(up_grid(&r)==grid);
  }
  dump(argc>1?argv[1]:0,"up-bank.bin",up_bank,sizeof up_bank[0]);
  memcpy(frozen_nor,nor,sizeof nor);unsigned er0=erases,pr0=bridge_programs;up_boot();CHECK(erases==er0 && bridge_programs==pr0 && !memcmp(frozen_nor,nor,sizeof nor));CHECK(up_value(up_rec(0),109)==values[i] && up_value(up_rec(1),109)==values[i]);
  CHECK(!up_rename(0,"RENAMED") && up_value(up_rec(0),109)==values[i]);
 }
 boot();project_capture(&p);old=p;old.magic=0x46554e44;old.sum=proj_sum(&old);CHECK(project_restore_runtime(&old)!=0);project_capture(&q);CHECK(!memcmp(&q,&p,sizeof p));
 for(unsigned i=0;i<2;i++){old=p;old.t[0].p[109]=i?128:-1;old.sum=proj_sum(&old);CHECK(!proj_pack(&valid,&old));CHECK(project_restore_runtime(&old)!=0);}
 for(unsigned e=0;e<NENGINES;e++)if(e!=ENGI_GRAIN){q=p;q.t[0].engine=e;q.t[0].p[109]=0;CHECK(proj_pack(&valid,&q));CHECK(proj_import(&old,&valid,sizeof valid));CHECK(old.t[0].p[109]==0);q.t[0].p[109]=1;CHECK(!proj_pack(&valid,&q));}
 for(unsigned ver=1;ver<=5;ver++)for(unsigned i=0;i<(ver>=4?NELEM(values):1u);i++){
  up_rec_t r;snprintf(name,sizeof name,"up-v%u-%u",ver,ver>=4?values[i]:0);CHECK(read_fixture_bytes(name,&r,sizeof r)==192);up_rec_t before=r;CHECK(up_valid(&r));int16_t v[118];up_values(&r,v);CHECK(v[109]==64 && !memcmp(&before,&r,sizeof r));CHECK(!up_put(2,&r));CHECK(!up_load(2) && TSEL->p[109]==64);
 }
 boot();TSEL->p[109]=0;CHECK(!up_store(0,"EXPLICIT"));CHECK(up_rec(0)->ver==6 && up_value(up_rec(0),109)==0);up_bank_t bank=up_bank[0];unsigned er=erases,pr=bridge_programs;
 uint8_t msg[4+2*118+32]={0,ENGI_GRAIN,'X',0};for(unsigned j=0;j<118;j++){msg[4+j*2]=0;msg[5+j*2]=64;}up_rec_t r;unsigned slot;CHECK(!up_parse(msg,sizeof msg,&r,&slot));CHECK(r.ver==4 && up_value(&r,109)==64);CHECK(up_put(0,&r)!=0);CHECK(!memcmp(&bank,&up_bank[0],sizeof bank) && erases==er && bridge_programs==pr);CHECK(!up_put(1,&r));CHECK(!up_load(1) && TSEL->p[109]==64);
 CHECK(!motion_param(109));CHECK(motion_put(TSEL,0,109,0,0)!=0);
 uint32_t entry=70u|ml_code(109)<<9;ml_put(0,entry);int16_t win=TSEL->p[109];CHECK(midi_learned(70,0));CHECK(TSEL->p[109]==win && ml_get(0)==entry);CHECK(ml_learn(70,0,109)==2 && ml_get(0)==entry && ml_cc_of(0,109)==0);
 up_rec(3)->used=UP_USED;up_rec(3)->ver=8;bank=up_bank[0];er=erases;pr=bridge_programs;CHECK(up_put(3,0)!=0 && up_rename(3,"NO")!=0);CHECK(grain_bank_readonly(&bank));memcpy(ED_BK_RAW,&bank,sizeof bank);ed_bk_id=6;ed_bk_len=ed_bk_pos=sizeof bank;ed_bk_crc=st_crc32(&bank,sizeof bank);CHECK(ed_bk_commit()!=0);CHECK(!memcmp(&bank,&up_bank[0],sizeof bank) && erases==er && bridge_programs==pr);
 boot();project_capture(&p);CHECK(proj_pack(&valid,&p));future=valid;uint32_t magic=0x46554e44;memcpy(future.raw,&magic,4);seal_raw(&future);
 for(unsigned s=0;s<4;s++){boot();CHECK(!st_save(OBJ_PROJECT0+s,&future,sizeof future));blocked(s,PS_UNSUPPORTED);}
 for(unsigned i=0;i<3;i++){boot();project_store_t bad=valid;if(i==0)bad.raw[300]^=1;if(i==1){bad.raw[66]=117;seal_raw(&bad);}CHECK(!st_save(OBJ_PROJECT0,&bad,i==2?3839:3840));blocked(0,PS_CORRUPT);}
 for(unsigned val=0;val<NELEM(values);val++){
  boot();snprintf(name,sizeof name,"v11-%u",values[val]);CHECK(read_fixture_bytes(name,&valid,sizeof valid)==3840);CHECK(!st_save(OBJ_PROJECT0,&valid,sizeof valid));CHECK(!st_save(OBJ_AUTOSAVE,&valid,sizeof valid));memcpy(frozen_nor,nor,sizeof nor);er=erases;pr=bridge_programs;proj_fetch(0);CHECK(!project_load(0));for(unsigned t=0;t<NTRK;t++)CHECK(trk[t].p[109]==64);CHECK(autosave_boot(1));for(unsigned t=0;t<NTRK;t++)CHECK(trk[t].p[109]==64);CHECK(erases==er && bridge_programs==pr && !memcmp(frozen_nor,nor,sizeof nor));
 }
 /* Loading old user records must never promote flash in place. */
 for(unsigned ver=1;ver<=5;ver++){
  boot();up_bank_t legacy={0};legacy.magic=UP_BANK_MAGIC;legacy.rsize=192;legacy.nslot=16;snprintf(name,sizeof name,"up-v%u-0",ver);CHECK(read_fixture_bytes(name,&legacy.r[0],192)==192);CHECK(!st_save(OBJ_UPRESET0,&legacy,sizeof legacy));memcpy(frozen_nor,nor,sizeof nor);er=erases;pr=bridge_programs;up_boot();CHECK(!up_load(0) && TSEL->p[109]==64);CHECK(erases==er && bridge_programs==pr && !memcmp(frozen_nor,nor,sizeof nor));
 }
 /* Undo and native preset copies preserve explicit zero rather than defaulting. */
 boot();TSEL->p[109]=0;apply_preset_to(TSEL,1);CHECK(TSEL->p[109]==64);undo_swap();CHECK(TSEL->p[109]==0);CHECK(!up_store(0,"ZERO"));TSEL->p[109]=127;CHECK(!up_store(1,"HIGH"));CHECK(!up_load(0) && TSEL->p[109]==0);CHECK(!up_store(2,"COPY") && up_value(up_rec(2),109)==0);
 /* PER4 provenance is deliberately unchanged: retain raw learned109 through
  * real settings NOR and the existing settings backup, but never execute it. */
 boot();entry=70u|(ml_code(109)<<9);ml_put(0,entry);settings_save();persist_t saved;CHECK(st_load(OBJ_SETTINGS,&saved,sizeof saved)==sizeof saved);CHECK(!memcmp(saved.favorites.factory[14],favorites.factory[14],32));uint8_t learned_raw[32];memcpy(learned_raw,favorites.factory[14],32);memset(favorites.factory[14],0,32);CHECK(settings_import(&saved,sizeof saved));CHECK(!memcmp(learned_raw,favorites.factory[14],32));memcpy(ED_BK_RAW,&saved,sizeof saved);ed_bk_id=1;ed_bk_len=ed_bk_pos=sizeof saved;ed_bk_crc=st_crc32(&saved,sizeof saved);CHECK(!ed_bk_commit());CHECK(ml_get(0)==entry);win=TSEL->p[109];CHECK(midi_learned(70,0) && TSEL->p[109]==win && ml_get(0)==entry);
 /* Existing invalid-length/future record bytes remain intact on normalization. */
 for(unsigned mode=0;mode<4;mode++){up_rec_t r={0};r.used=UP_USED;r.ver=6;r.engine=ENGI_GRAIN;r.np=118;memcpy(r.name,"VALID",5);for(unsigned j=0;j<118;j++)up_set_value(&r,j,0);if(mode==0)r.ver=8;if(mode==1)r.np=117;if(mode==2)r.packed[109]=63;if(mode==3)r.packed[109]=192;CHECK(!up_valid(&r));up_bank_t b=up_bank[0];b.magic=UP_BANK_MAGIC;b.rsize=192;b.nslot=16;b.r[3]=r;up_bank[0]=b;up_bank_check(0,sizeof b);CHECK(!memcmp(&r,up_rec(3),192) && grain_bank_readonly(&up_bank[0]));}
 printf("WIN I1-R persistence/NOR: %u checks passed; versions, provenance, retained restore, all tracks and no-write protections\n",checks);return 0;
}
