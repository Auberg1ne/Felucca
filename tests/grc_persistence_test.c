/* SPDX-License-Identifier: GPL-3.0-only */
#define BRIDGE_TEST_NO_MAIN 1
#include "project_downgrade_bridge_test.c"
#include "grc_migration_model.h"
static project_t q, next, sentinel;
static unsigned load_fixture(const char *name,void *out,unsigned cap){char path[192];snprintf(path,sizeof path,"tests/fixtures/grain-win-i1r/%s.bin",name);FILE*f=fopen(path,"rb");CHECK(f);unsigned n=fread(out,1,cap,f);CHECK(feof(f)||fgetc(f)==EOF);fclose(f);return n;}
int main(int argc,char **argv){
 CHECK(P_GRC==118 && P_COUNT==119 && P_E0==110 && P_E7==117);
 CHECK(sizeof(project118_t)==4840 && sizeof(project_t)==4848 && sizeof(project_store_t)==3840 && sizeof(up_rec_t)==192);
 for(unsigned e=0;e<NENGINES;e++)for(unsigned id=0;id<128;id++){
  const param_desc_t*d=param_desc_of(e,id);CHECK(d);if(id==118)CHECK(d->min==0 && d->max==(e==ENGI_GRAIN?100:0) && d->def==0);
  else if(id>=119)CHECK(d->min==0 && d->max==0);
 }
 for(unsigned np=8;np<=144;np++){
  int16_t in[144],def[119],out[120],model[119];for(unsigned i=0;i<144;i++)in[i]=i;for(unsigned i=0;i<119;i++)def[i]=-1;
  out[119]=1234;params_by_count(out,in,np,def);CHECK(mc_historical(model,in,144,np,def));CHECK(!memcmp(out,model,sizeof model));CHECK(out[119]==1234);
 }
 boot();for(unsigned t=0;t<4;t++)CHECK(trk[t].p[118]==0);
 static uint8_t raw[4848];char name[64];
 for(unsigned version=1;version<=12;version++){
  unsigned n;if(version<=10){snprintf(name,sizeof name,"v%u",version);n=load_fixture(name,raw,sizeof raw);}
  else if(version==11)n=load_fixture("v11-127",raw,sizeof raw);
  else{n=sizeof valid;FILE*f=fopen("tests/fixtures/grain-win-i1/proposed-func.bin","rb");CHECK(f);CHECK(fread(raw,1,n,f)==n);fclose(f);}
  uint32_t hash=st_crc32(raw,n);CHECK(proj_import(&q,raw,n));CHECK(st_crc32(raw,n)==hash);for(unsigned t=0;t<4;t++)CHECK(q.t[t].p[118]==0);
 }
 /* Frozen native4840 import and restore-last migration cannot use new sizeof. */
 for(unsigned c=0;c<7;c++){
  snprintf(name,sizeof name,"retained-%u",(unsigned[]){0,1,63,64,65,126,127}[c]);unsigned n=load_fixture(name,raw,sizeof raw);CHECK(n==4840);
  CHECK(proj_import(&q,raw,n));for(unsigned t=0;t<4;t++)CHECK(q.t[t].p[118]==0 && q.t[t].p[109]==64);
  CHECK(!project_restore_runtime((const project_t*)raw));for(unsigned t=0;t<4;t++)CHECK(trk[t].p[118]==0);
  /* Frozen FUNC/native4840 preserves explicit WIN, still has no C provenance. */
  project118_t *native=(project118_t*)raw;native->magic=PROJ_MAGIC_V12;unsigned w=(unsigned[]){0,1,63,64,65,126,127}[c];
  for(unsigned t=0;t<4;t++)native->t[t].p[109]=w;native->sum=proj_hash(native,sizeof *native-4);
  CHECK(proj_import(&next,native,sizeof *native));for(unsigned t=0;t<4;t++)CHECK(next.t[t].p[118]==0&&next.t[t].p[109]==(int)w);
  CHECK(!project_restore_runtime((const project_t*)raw));for(unsigned t=0;t<4;t++)CHECK(trk[t].p[118]==0&&trk[t].p[109]==(int)w);
 }
 for(unsigned ver=1;ver<=7;ver++){
  up_rec_t r;if(ver<=5){snprintf(name,sizeof name,"up-v%u-%u",ver,ver>=4?127:0);CHECK(load_fixture(name,&r,sizeof r)==192);}
  else{boot();CHECK(!up_store(0,"CURRENT"));r=*up_rec(0);r.ver=ver;r.np=118;}
  CHECK(up_valid(&r));int16_t values[119];up_values(&r,values);CHECK(values[118]==0);CHECK(!up_put(0,&r));CHECK(!up_load(0));CHECK(TSEL->p[118]==0);
 }
 up_rec_t historical={0};historical.used=UP_USED;historical.ver=4;historical.engine=ENGI_GRAIN;historical.np=119;memcpy(historical.name,"COUNT119",8);
 for(unsigned i=0;i<119;i++)up_set_value(&historical,i,i);CHECK(up_valid(&historical));int16_t values[119];up_values(&historical,values);
 for(unsigned i=0;i<8;i++){int16_t expected=param_fit(param_desc_of(ENGI_GRAIN,110+i),111+i);CHECK(values[110+i]==expected);}CHECK(values[118]==0 && values[109]==64);
 for(unsigned v=0;v<4;v++)for(unsigned grid=0;grid<2;grid++){
  unsigned c=(unsigned[]){0,1,55,100}[v];boot();for(unsigned t=0;t<4;t++)trk[t].p[118]=c;project_capture(&q);CHECK(proj_pack(&valid,&q));CHECK(*(uint32_t*)valid.raw==PROJ_MAGIC_V13 && valid.raw[66]==119);CHECK(proj_import(&next,&valid,3840));CHECK(!memcmp(&q,&next,sizeof q));
  CHECK(!project_restore_runtime(&next));for(unsigned t=0;t<4;t++)CHECK(trk[t].p[118]==c);
  CHECK(!project_save(0));CHECK(!project_load(0));CHECK(!restore(1,&valid,3840));CHECK(!project_load(1));for(unsigned t=0;t<4;t++)CHECK(trk[t].p[118]==c);
  CHECK(!up_store(0,"C"));up_rec_t r=*up_rec(0);r.ver=grid?9:8;CHECK(up_valid(&r));CHECK(!up_put(0,&r));CHECK(!up_load(0));CHECK(TSEL->p[118]==c);CHECK(up_grid(&r)==grid);
  up_boot();CHECK(up_used(0));CHECK(!up_load(0));CHECK(TSEL->p[118]==c);
  memcpy(ED_BK_RAW,&up_bank[0],sizeof up_bank[0]);ed_bk_id=6;ed_bk_len=ed_bk_pos=sizeof up_bank[0];ed_bk_crc=st_crc32(ED_BK_RAW,ed_bk_len);CHECK(!ed_bk_commit());CHECK(up_value(up_rec(0),118)==c);
  CHECK(proj_import(&q,&next,sizeof next));CHECK(!memcmp(&q,&next,sizeof q));
  if(argc>1 && c==100 && !grid){const void*data[]={&valid,&up_bank[0],&next};unsigned sizes[]={3840,3080,4848};const char*names[]={"fund-c100.bin","bank-c100.bin","runtime-c100.bin"};for(unsigned j=0;j<3;j++){char path[512];snprintf(path,sizeof path,"%s/%s",argv[1],names[j]);FILE*f=fopen(path,"wb");CHECK(f&&fwrite(data[j],1,sizes[j],f)==sizes[j]);fclose(f);}}
  sentinel=next;for(unsigned n=0;n<3840;n++){q=sentinel;CHECK(!proj_import(&q,&valid,n));CHECK(!memcmp(&q,&sentinel,sizeof q));}
 }
 /* Bad alignment/null/truncated native inputs cannot partially publish state. */
 boot();project_capture(&next);sentinel=next;uint8_t unaligned[sizeof next+1];memcpy(unaligned+1,&next,sizeof next);
 CHECK(!proj_import(&q,unaligned+1,sizeof next));CHECK(!proj_import(&q,0,sizeof next));CHECK(!proj_import(0,&next,sizeof next));
 for(unsigned n=3840;n<sizeof next;n++){q=sentinel;CHECK(!proj_import(&q,&next,n));CHECK(!memcmp(&q,&sentinel,sizeof q));}
 /* Frozen motion constants and no automation/MIDI exposure for118. */
 for(unsigned np=8;np<=118;np++)for(unsigned id=0;id<np;id++)for(unsigned lock=0;lock<2;lock++){
  motion_store_t m={0};m.count=1;m.event[0].param=id|(lock?128:0);uint8_t expected;CHECK(mc_motion(&expected,m.event[0].param,np,0));CHECK(proj_motion_ids(&m,np));CHECK(m.event[0].param==expected);
 }
 CHECK(!motion_param(118));CHECK(ml_code(118)==0);CHECK(ml_learn(70,0,118)==2);CHECK(ml_id(127)==117);
 for(unsigned code=0;code<128;code++)CHECK(ml_id(code)!=118);
 for(unsigned e=0;e<NENGINES;e++)for(unsigned ver=6;ver<=9;ver++){
  up_rec_t r={0};r.used=UP_USED;r.ver=ver;r.engine=e;r.np=ver>=8?119:118;r.name[0]='R';
  for(unsigned id=0;id<r.np;id++)up_set_value(&r,id,param_desc_of(e,id)->def);CHECK(up_valid(&r));CHECK(up_engine_base(&r)==110);
  for(unsigned j=0;j<8;j++)CHECK(up_value(&r,up_engine_base(&r)+j)==param_desc_of(e,110+j)->def);
 }
 /* No active motion bit or legacy controller code can turn C into automation. */
 /* PERC migration must use E0..E7, not the new C tail byte. */
 for(unsigned ver=4;ver<=9;ver++){
  if(ver==5||ver==6||ver==7)continue;
  up_rec_t r={0};r.used=UP_USED;r.ver=ver;r.engine=ENGI_SAMPLE;r.np=119;r.name[0]='P';
  for(unsigned id=0;id<119;id++)up_set_value(&r,id,0);
  up_set_value(&r,ver>=8?110:111,4);CHECK(up_valid(&r));up_migrate(&r);CHECK(r.engine==ENGI_DRUM);
  up_values(&r,values);CHECK(values[118]==0);
 }
 TSEL->p[118]=100;motion_active[0][118/32]|=1u<<(118%32);CHECK(motion_base_value(TSEL,118)==100);motion_restore(TSEL);CHECK(TSEL->p[118]==100);
 motion_store_t forbidden={0};forbidden.count=1;forbidden.event[0].param=118;CHECK(!motion_valid(&forbidden));
 CHECK(motion_put(TSEL,0,118,55,0)!=0);CHECK(TSEL->p[118]==100);
 /* Future/corrupt and invalid-C imports must leave the destination untouched. */
 boot();project_capture(&next);next.t[0].p[118]=101;CHECK(!proj_pack(&valid,&next));
 CHECK(!up_store(0,"GOOD"));up_rec_t bad=*up_rec(0);bad.packed[118]=165;CHECK(!up_valid(&bad));bad=*up_rec(0);bad.ver=10;CHECK(!up_valid(&bad));
 printf("GRC persistence: %u checks passed; historical mapping, formats13/8/9, atomic imports and C round trips\n",checks);return 0;
}
