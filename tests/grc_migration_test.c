/* SPDX-License-Identifier: GPL-3.0-only */
#define BRIDGE_TEST_NO_MAIN 1
#include "project_downgrade_bridge_test.c"
#include "grc_migration_model.h"
static int old_project(const uint8_t*p,size_t n){project_t q;return n<=INT32_MAX&&proj_import(&q,p,(int)n);}
static int old_preset(const uint8_t*p,size_t n){up_rec_t r;if(n!=192)return 0;memcpy(&r,p,192);return up_valid(&r);}
static unsigned load(const char*file,void*out,unsigned cap){FILE*f=fopen(file,"rb");CHECK(f);unsigned n=fread(out,1,cap,f);CHECK(feof(f)||fgetc(f)==EOF);fclose(f);return n;}
static void mappings(void){
 CHECK(P_COUNT==118&&P_E0==110&&P_E7==117&&NENGINES==14);CHECK(grain_grid_param(1,0,3)==255);
 int16_t input[144],def[118],got[119],prod[118],snapshot[119];for(unsigned i=0;i<144;i++)input[i]=(int16_t)((int)i-64);for(unsigned i=0;i<118;i++)def[i]=-17;
 for(unsigned np=8;np<=144;np++){
  CHECK(mc_historical(got,input,144,np,def));params_by_count(prod,input,np,def);CHECK(!memcmp(got,prod,sizeof prod)&&got[118]==0);
  for(unsigned n=0;n<np;n++){memset(got,0x55,sizeof got);memcpy(snapshot,got,sizeof got);CHECK(!mc_historical(got,input,n,np,def)&&!memcmp(got,snapshot,sizeof got));}
 }
 up_rec_t r={0};r.used=UP_USED;r.ver=4;r.engine=8;r.np=119;memcpy(r.name,"LEGACY",6);for(unsigned i=0;i<119;i++)up_set_value(&r,i,(int16_t)i);
 CHECK(up_valid(&r));up_params(&r,prod,def);CHECK(prod[110]==111&&prod[117]==118);
 for(unsigned i=0;i<119;i++)input[i]=(int16_t)i;CHECK(mc_historical(got,input,119,119,def));CHECK(got[110]==111&&got[117]==118&&got[118]==0);
 input[118]=55;CHECK(mc_stable(got,input,119,119,8));for(unsigned i=0;i<119;i++)CHECK(got[i]==input[i]);
 for(unsigned np=0;np<145;np++)if(np!=119){memcpy(snapshot,got,sizeof got);CHECK(!mc_stable(got,input,144,np,8)&&!memcmp(snapshot,got,sizeof got));}
 for(int c=-1;c<=101;c++){input[118]=(int16_t)c;int ok=mc_stable(got,input,119,119,8);CHECK(ok==(c>=0&&c<=100));}
 for(unsigned id=0;id<118;id++)for(unsigned np=8;np<=118;np++)for(unsigned lock=0;lock<2;lock++){
  uint8_t dst=0xaa;unsigned raw=id|(lock?128:0);int ok=mc_motion(&dst,raw,np,0);CHECK(ok==(id<np));
  if(ok){motion_store_t m={0};m.count=1;m.event[0].param=(uint8_t)raw;CHECK(proj_motion_ids(&m,np));CHECK(dst==m.event[0].param);}
 }
 for(unsigned raw=0;raw<256;raw++){uint8_t dst=0xaa;int ok=mc_motion(&dst,raw,119,1);CHECK(ok==((raw&127)<118));if(ok)CHECK(dst==raw);}
 CHECK(ML_E0==120&&P_COUNT-P_E0==8);CHECK(ml_code(118)==128);CHECK((uint16_t)(ml_code(118)<<9)==0);CHECK(ml_id(119)==P_COUNT);
}
static void historical(void){
 uint8_t raw[4848];char path[160];project_t q;mc_project current,proposed,decoded;mc_runtime119 runtime;int16_t c[4]={0},outc[4];
 for(unsigned version=1;version<=12;version++){
  unsigned n;if(version<=10){snprintf(path,sizeof path,"tests/fixtures/grain-win-i1r/v%u.bin",version);n=load(path,raw,sizeof raw);}
  else if(version==11)n=load("tests/fixtures/grain-win-i1r/v11-127.bin",raw,sizeof raw);
  else{boot();project_capture(&q);CHECK(proj_pack((project_store_t*)&current,&q));memcpy(raw,current.raw,3840);n=3840;}
  uint32_t before=mc_hash(raw,n);CHECK(proj_import(&q,raw,n));CHECK(before==mc_hash(raw,n));CHECK(proj_pack((project_store_t*)&current,&q));
  CHECK(mc_project_pack(&proposed,&current,c,old_project));CHECK(mc_project_unpack(&decoded,outc,proposed.raw,3840,old_project));
  for(unsigned t=0;t<4;t++)CHECK(outc[t]==0);CHECK(!memcmp(decoded.raw,current.raw,3840));
  CHECK(mc_runtime_upgrade(&runtime,&q,sizeof q));for(unsigned t=0;t<4;t++){CHECK(runtime.track[t].p[118]==0);CHECK(!memcmp(runtime.track[t].p,q.t[t].p,236));}
 }
 for(unsigned v=0;v<128;v+=1){
  if(v!=0&&v!=1&&v!=63&&v!=64&&v!=65&&v!=126&&v!=127)continue;
  snprintf(path,sizeof path,"tests/fixtures/grain-win-i1r/retained-%u.bin",v);unsigned n=load(path,raw,sizeof raw);CHECK(n==4840);uint32_t before=mc_hash(raw,n);
  CHECK(proj_import(&q,raw,n));CHECK(mc_runtime_upgrade(&runtime,&q,sizeof q));CHECK(before==mc_hash(raw,n));for(unsigned t=0;t<4;t++)CHECK(runtime.track[t].p[118]==0&&runtime.track[t].p[109]==64);
 }
 for(unsigned version=1;version<=7;version++){
  up_rec_t r;unsigned n;if(version<=5){snprintf(path,sizeof path,"tests/fixtures/grain-win-i1r/up-v%u-%u.bin",version,version>=4?127:0);n=load(path,&r,sizeof r);}
  else{boot();CHECK(!up_store(0,"CURRENT"));r=*up_rec(0);r.ver=(uint8_t)version;n=192;}
  CHECK(n==192&&up_valid(&r));uint32_t before=mc_hash(&r,192);int16_t input[144],def[118]={0},got[119],actual[118];
  for(unsigned i=0;i<r.np;i++)input[i]=up_value(&r,i);CHECK(mc_historical(got,input,144,r.np,def));if(version<=5)got[109]=r.engine==8?64:0;
  up_params(&r,actual,def);CHECK(!memcmp(got,actual,236)&&got[118]==0);CHECK(mc_hash(&r,192)==before);
 }
}
static void versions_and_storage(void){
 CHECK(sizeof(project_t)==4840&&sizeof(proj_slot[0])==3840&&sizeof(up_rec_t)==192);
 CHECK(offsetof(project_t,t)==66&&offsetof(project_t,sum)==4836&&sizeof(step_t)==11);
 CHECK(sizeof(chain_config_t)==84&&sizeof(motion_store_t)==388&&NSTEP==64&&G_COUNT==27);
 CHECK(offsetof(mc_runtime119,track)==66&&offsetof(mc_runtime119,sum)==4844);
 CHECK(68+4*(118+2+576)+472==3324&&3373-3324==49);CHECK(68+4*(119+2+576)+472==3328&&3373-3328==45);
 CHECK(16+119<16+143&&192==16+144+32); /* new parameter does not touch category/pattern */
 for(unsigned ver=0;ver<256;ver++){
  up_rec_t r={0};r.used=UP_USED;r.ver=(uint8_t)ver;r.engine=8;r.np=ver<4?69:118;r.name[0]='V';for(unsigned i=0;i<r.np;i++)up_set_value(&r,i,0);
  CHECK(up_valid(&r)==(ver>=1&&ver<=7));
 }
 boot();project_t p;project_capture(&p);mc_project old;CHECK(proj_pack((project_store_t*)&old,&p));
 for(unsigned i=0;i<256;i++){mc_project bad=old;mc_w32(bad.raw,UINT32_C(0x46554e00)+i);mc_seal(&bad);
  if(i>='1'&&i<='9')continue;if(i=='A'||i=='B'||i=='C')continue;
  CHECK(!old_project(bad.raw,3840));
 }
 CHECK(PROJ_MAGIC_V12==MC_FUNC&&UP_VER==6&&UP_VER_GRID==7);CHECK(!old_project(NULL,0));
}
static void candidates(void){
 boot();project_t p,q;project_capture(&p);mc_project old,next,back,sentinel;CHECK(proj_pack((project_store_t*)&old,&p));
 const int values[]={0,1,55,100};int16_t c[4],got[4],untouched[4];
 for(unsigned base=0;base<4;base++){
  for(unsigned t=0;t<4;t++)c[t]=values[(base+t)%4];CHECK(mc_project_pack(&next,&old,c,old_project));
  CHECK(!old_project(next.raw,3840));CHECK(mc_project_unpack(&back,got,next.raw,3840,old_project));CHECK(!memcmp(c,got,sizeof c));CHECK(proj_import(&q,back.raw,3840));
  for(unsigned t=0;t<4;t++)CHECK(!memcmp(p.t[t].p,q.t[t].p,236));
  for(unsigned n=0;n<3840;n++){memset(&back,0x55,sizeof back);sentinel=back;memset(got,0x33,sizeof got);memcpy(untouched,got,sizeof got);CHECK(!mc_project_unpack(&back,got,next.raw,n,old_project));CHECK(!memcmp(&back,&sentinel,sizeof back)&&!memcmp(got,untouched,sizeof got));}
  for(unsigned i=0;i<3840;i++){mc_project bad=next;bad.raw[i]^=1;CHECK(!mc_project_unpack(&back,got,bad.raw,3840,old_project));}
  for(unsigned mode=0;mode<4;mode++){
   mc_project bad=next;if(mode==0)bad.raw[68+118]=63;if(mode==1)bad.raw[68+118]=165;if(mode==2)bad.raw[66]=118;if(mode==3)mc_w32(bad.raw,MC_FUND_CANDIDATE+1);mc_seal(&bad);
   CHECK(!mc_project_unpack(&back,got,bad.raw,3840,old_project));
  }
  boot();unsigned er=erases,pr=bridge_programs;freeze();CHECK(restore(0,next.raw,3840)!=0);unchanged(er,pr);
  /* Physical NOR downgrade oracle for ACTUAL candidate payload, all slots. */
  memcpy(&future,&next,3840);memcpy(&valid,&old,3840);
  for(unsigned slot=0;slot<4;slot++){boot();CHECK(!st_save(OBJ_PROJECT0+slot,&future,3840));blocked(slot,PS_UNSUPPORTED);}
 }
 for(unsigned grid=0;grid<2;grid++)for(unsigned v=0;v<4;v++){
  boot();CHECK(!up_store(0,"CANDIDATE"));mc_preset original,record,decoded,freeze_record;memcpy(&original,up_rec(0),192);original.raw[1]=(uint8_t)(6+grid);
  original.raw[16+143]=3;
  for(unsigned w=0;w<3;w++){mc_preset explicit_win=original,win_record,win_back;int win_c;explicit_win.raw[16+109]=(uint8_t)(64+(unsigned[]){0,64,127}[w]);
   CHECK(mc_preset_pack(&win_record,&explicit_win,values[v],old_preset));CHECK(mc_preset_unpack(&win_back,&win_c,win_record.raw,192,old_preset));CHECK(win_back.raw[16+109]==explicit_win.raw[16+109]&&win_c==values[v]);}
  CHECK(mc_preset_pack(&record,&original,values[v],old_preset));CHECK(record.raw[1]==8+grid&&record.raw[3]==119);
  int decoded_c=-1;CHECK(mc_preset_unpack(&decoded,&decoded_c,record.raw,192,old_preset));CHECK(decoded_c==values[v]);CHECK(!memcmp(decoded.raw+16,original.raw+16,118));CHECK(!memcmp(decoded.raw+16+119,original.raw+16+119,192-16-119));
  for(unsigned n=0;n<192;n++){memset(&decoded,0x44,192);freeze_record=decoded;decoded_c=-77;CHECK(!mc_preset_unpack(&decoded,&decoded_c,record.raw,n,old_preset));CHECK(decoded_c==-77&&!memcmp(&decoded,&freeze_record,192));}
  for(unsigned mode=0;mode<5;mode++){mc_preset bad=record;if(mode==0)bad.raw[1]=10;if(mode==1)bad.raw[3]=118;if(mode==2)bad.raw[16+118]=63;if(mode==3)bad.raw[16+118]=165;if(mode==4)bad.raw[16+109]=63;CHECK(!mc_preset_unpack(&decoded,&decoded_c,bad.raw,192,old_preset));}
  /* Structurally legal byte changes need the bank/transport CRC: records have no checksum. */
  mc_preset changed=record;changed.raw[16]^=1;CHECK(mc_preset_unpack(&decoded,&decoded_c,changed.raw,192,old_preset));CHECK(decoded.raw[16]!=original.raw[16]);
  up_bank_t bank=up_bank[0];memcpy(&bank.r[0],&record,192);CHECK(!up_valid(&bank.r[0]));CHECK(!st_save(OBJ_UPRESET0,&bank,sizeof bank));up_boot();CHECK(grain_bank_readonly(&up_bank[0]));unsigned er=erases,pr=bridge_programs;freeze();
  CHECK(up_put(0,NULL)!=0&&up_rename(0,"NO")!=0);unchanged(er,pr);CHECK(!memcmp(&bank,&up_bank[0],sizeof bank));
  memcpy(ED_BK_RAW,&bank,sizeof bank);ed_bk_id=6;ed_bk_len=ed_bk_pos=sizeof bank;ed_bk_crc=st_crc32(&bank,sizeof bank);CHECK(ed_bk_commit()!=0);unchanged(er,pr);CHECK(!memcmp(&bank,&up_bank[0],sizeof bank));
  boot();CHECK(!up_store(0,"VALID"));up_bank_t valid_bank=up_bank[0];er=erases;pr=bridge_programs;freeze();memcpy(ED_BK_RAW,&bank,sizeof bank);ed_bk_id=6;ed_bk_len=ed_bk_pos=sizeof bank;ed_bk_crc=st_crc32(&bank,sizeof bank);CHECK(ed_bk_commit()!=0);unchanged(er,pr);CHECK(!memcmp(&valid_bank,&up_bank[0],sizeof valid_bank));
 }
}
static void provenance_and_runtime(void){
 /* Existing wire lengths are fixed118. An unmarked119-value PUT is rejected
  * by the real parser, rather than becoming a new schema by its count. */
 uint8_t message[4+2*119+32]={0,8,'X',0};for(unsigned j=0;j<119;j++)message[5+2*j]=64;
 up_rec_t rejected,original;memset(&rejected,0x55,sizeof rejected);original=rejected;unsigned slot=99;
 CHECK(up_parse(message,sizeof message,&rejected,&slot)!=0);CHECK(!memcmp(&original,&rejected,sizeof rejected));

 int16_t in[144]={0},now[119]={0},out[119],before[119],tracks[4][119]={{0}};now[109]=in[109]=64;
 for(unsigned c=0;c<=100;c++)for(unsigned t=0;t<4;t++){int16_t snapshot[4][119];memcpy(snapshot,tracks,sizeof tracks);CHECK(mc_scalar(tracks,t,118,(int)c,8,1));for(unsigned other=0;other<4;other++)if(other!=t)CHECK(!memcmp(snapshot[other],tracks[other],238));CHECK(!memcmp(snapshot[t],tracks[t],236));CHECK(tracks[t][118]==c);for(unsigned type=0;type<4;type++){tracks[t][103]=(int16_t)type;CHECK(tracks[t][118]==c);}}
 for(unsigned mode=0;mode<6;mode++){memcpy(before,tracks[0],238);CHECK(!mc_scalar(tracks,mode==0?4:0,mode==1?117:118,mode==2?-1:mode==3?101:55,mode==4?0:8,mode==5?0:1));CHECK(!memcmp(before,tracks[0],238));}
 for(unsigned legacy_n=118;legacy_n<=119;legacy_n++)for(unsigned byte=0;byte<128;byte++){
  in[109]=(int16_t)byte;in[118]=55;CHECK(mc_bulk(out,in,legacy_n,8,0,0,now));CHECK(out[109]==64&&out[118]==0);
 }
 in[109]=64;in[118]=55;CHECK(mc_bulk(out,in,119,8,1,1,now));CHECK(out[118]==55);
 now[109]=127;CHECK(!mc_bulk(out,in,119,8,1,1,now));now[109]=64;
 for(unsigned engine=0;engine<14;engine++)if(engine!=8){in[109]=0;in[118]=0;CHECK(mc_stable(out,in,119,119,engine));in[118]=1;CHECK(!mc_stable(out,in,119,119,engine));}in[109]=64;in[118]=55;
 for(unsigned mode=0;mode<7;mode++){
  memset(out,0x55,sizeof out);memcpy(before,out,sizeof out);now[118]=mode==0?55:0;now[109]=mode==1?127:64;in[109]=mode==4?0:64;
  CHECK(!mc_bulk(out,in,mode==5?SIZE_MAX:119,8,mode<2?0:mode==6?2:1,mode==2?0:mode==3?2:1,now));CHECK(!memcmp(out,before,sizeof out));
 }
 boot();project_t p;project_capture(&p);mc_runtime119 runtime,copy;CHECK(mc_runtime_upgrade(&runtime,&p,4840));
 for(unsigned n=0;n<4840;n++){memset(&copy,0x55,sizeof copy);mc_runtime119 before_copy=copy;CHECK(!mc_runtime_upgrade(&copy,&p,n)&&!memcmp(&copy,&before_copy,sizeof copy));}
 for(unsigned t=0;t<4;t++){CHECK(runtime.track[t].p[118]==0);CHECK(!memcmp(runtime.track[t].p,p.t[t].p,236));}
 const int candidate_values[]={0,1,55,100};
 for(unsigned v=0;v<4;v++){
  for(unsigned t=0;t<4;t++)runtime.track[t].p[118]=(int16_t)candidate_values[(v+t)%4];runtime.sum=mc_hash(&runtime,4844);
  CHECK(mc_runtime_read(&copy,&runtime,4848,old_project));CHECK(!memcmp(&runtime,&copy,4848));CHECK(!old_project((const uint8_t*)&runtime,4848));
 }
 for(unsigned n=0;n<4848;n++){memset(&copy,0x55,sizeof copy);mc_runtime119 snapshot=copy;CHECK(!mc_runtime_read(&copy,&runtime,n,old_project)&&!memcmp(&copy,&snapshot,4848));}
 runtime.track[0].p[118]=101;runtime.sum=mc_hash(&runtime,4844);CHECK(!mc_runtime_read(&copy,&runtime,4848,old_project));runtime.track[0].p[118]=0;
 runtime.sum=mc_hash(&runtime,4844);
 mc_runtime119 undo=runtime;runtime.track[0].p[118]=55;copy=runtime;runtime=undo;CHECK(runtime.track[0].p[118]==0&&copy.track[0].p[118]==55); /* model snapshot, not production undo */
}
static void bank_boundaries(void){
 for(unsigned b=0;b<2;b++)for(unsigned pos=0;pos<16;pos+=15){
  boot();unsigned slot=b*16+pos;CHECK(!up_store(slot,"BOUNDARY"));mc_preset old,record;memcpy(&old,up_rec(slot),192);old.raw[1]=7;CHECK(mc_preset_pack(&record,&old,100,old_preset));
  up_bank_t bank=up_bank[b];memcpy(&bank.r[pos],record.raw,192);CHECK(sizeof bank==3080);CHECK(!st_save(OBJ_UPRESET0+b,&bank,sizeof bank));up_boot();
  CHECK(grain_bank_readonly(&up_bank[b])&&!grain_bank_readonly(&up_bank[1-b]));up_bank_t snapshots[2];memcpy(snapshots,up_bank,sizeof snapshots);
  unsigned er=erases,pr=bridge_programs;freeze();CHECK(up_store(slot,"NO")!=0&&up_put(slot,NULL)!=0&&up_rename(slot,"NO")!=0);unchanged(er,pr);CHECK(!memcmp(snapshots,up_bank,sizeof snapshots));
 }
}
/* Expected FAIL-OPEN behavior is evidence, never labelled a safety pass.
 * The baseline protects valid future records but not malformed bank ownership. */
static void known_downgrade_gaps(void){
 for(unsigned mode=0;mode<3;mode++){
  boot();CHECK(!up_store(0,"SEED"));mc_preset old,record;memcpy(&old,up_rec(0),192);CHECK(mc_preset_pack(&record,&old,55,old_preset));
  boot();up_bank_t bank=up_bank[0];bank.magic=UP_BANK_MAGIC;bank.rsize=192;bank.nslot=16;memcpy(&bank.r[0],record.raw,192);
  if(mode==1)bank.magic^=1;CHECK(!st_save(OBJ_UPRESET0,&bank,mode==0?sizeof bank-1:sizeof bank));
  if(mode==2)nor[st_sector(OBJ_UPRESET0,0)+ST_PAYLOAD_OFF+20]^=1;
  up_boot();CHECK(!grain_bank_readonly(&up_bank[0]));unsigned er=erases,pr=bridge_programs;freeze();
  CHECK(!up_store(0,"REPLACED"));CHECK(erases>er&&bridge_programs>pr);CHECK(memcmp(frozen_nor,nor,sizeof nor));
 }
 puts("KNOWN DOWNGRADE LIMITATION: baseline permits replacement after truncated bank, unknown bank header or storage CRC failure (3 simulated counterexamples)");
}
int main(void){mappings();historical();versions_and_storage();candidates();provenance_and_runtime();bank_boundaries();known_downgrade_gaps();printf("GRC test-only proof: %u checks passed; legacy counts/119, stable119, raw sizes, candidates13/8/9, NOR oracle and provenance\n",checks);return 0;}
