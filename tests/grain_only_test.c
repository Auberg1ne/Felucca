/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"grain-only %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 REQUIRE(FELUCCA_GRAIN_ONLY && NENG_SHOWN==1 && NENGINES==14 && P_COUNT==118 && sizeof(project_store_t)==3840);
 ui_power_on();for(unsigned t=0;t<NTRK;t++)REQUIRE(trk[t].engine==ENGI_GRAIN && trk[t].eng_req==ENGI_GRAIN && TRK_DEF[t][0]==ENGI_GRAIN);
 for(int d=-32;d<=32;d++)REQUIRE(eng_vis((unsigned)(d+32))==ENGI_GRAIN && eng_step(ENGI_GRAIN,d)==ENGI_GRAIN);
 for(unsigned e=0;e<NENGINES;e++){
  ui_power_on();int16_t before[P_COUNT];memcpy(before,TSEL->p,sizeof before);unsigned panic=panic_req;
  if(e==ENGI_GRAIN)continue;
  set_engine_of(TSEL,e);REQUIRE(TSEL->eng_req==ENGI_GRAIN && !memcmp(before,TSEL->p,sizeof before) && panic_req==panic);
  select_engine(e);REQUIRE(TSEL->eng_req==ENGI_GRAIN);
 }
 ui_power_on();press(B_EDIT);for(unsigned r=0;r<3;r++){REQUIRE(grain_edit_row==r);for(unsigned c=0;c<4;c++){int16_t*vp;const param_desc_t*d=page_desc(cur_page(),c,&vp);REQUIRE((grain_grid_param(0,r,c)==255)==!d);}turn(EN_SELECT,1);}
 frames(400);press(B_EDIT);REQUIRE(grain_grid_page(cur_page())==2);
 for(unsigned type=0;type<4;type++){TSEL->p[P_GTYPE]=type;REQUIRE(!strcmp(track_desc(TSEL,P_GA)->label,(const char*[]){"DRIFT","HOLD","RATE","INTERVAL"}[type]));}
 for(unsigned src=0;src<15;src++){TSEL->p[P_E0]=src;REQUIRE(track_desc(TSEL,P_E0)->max==14 && TSEL->eng_req==ENGI_GRAIN);}
 static project_t p,q,after;static project_store_t raw,copy;ui_power_on();project_capture(&p);
 for(unsigned e=0;e<NENGINES;e++)if(e!=ENGI_GRAIN){
  q=p;q.t[2].engine=e;for(unsigned id=0;id<P_COUNT;id++)q.t[2].p[id]=(id*7)%128;q.sum=proj_sum(&q);
  REQUIRE(proj_pack(&raw,&q));copy=raw;proj_slot[0]=raw;
  REQUIRE(project_restore_runtime(&q)!=0);project_capture(&after);REQUIRE(!memcmp(&p,&after,sizeof p));
  REQUIRE(project_load(0)!=0 && !memcmp(&copy,&proj_slot[0],sizeof copy));
  REQUIRE(project_save(0)!=0 && project_rename(0,"NO")!=0 && project_erase(0)!=0);
  REQUIRE(!memcmp(&copy,&proj_slot[0],sizeof copy));
  chain_config.count=1;chain_config.row[0]=chain_row_of(0,1);REQUIRE(chain_prepare()==7 && !chain.armed && !song.playing);chain_defaults(&chain_config);
 }
 /* The historical KEEP marker invokes DRUM recovery even if a crafted record says GRAIN. */
 q=p;q.t[0].preset=PROJ_DEF_KEEP;q.sum=proj_sum(&q);REQUIRE(project_restore_runtime(&q)!=0);
 project_capture(&after);REQUIRE(!memcmp(&p,&after,sizeof p));
 ui_power_on();REQUIRE(!project_save(1) && !project_load(1));
 /* Mixed user banks are retained, but only GRAIN records enter sound browsing. */
 for(unsigned k=0;k<3;k++){unsigned rc=up_store(k,"GRAIN");REQUIRE(rc==0 || rc==3);}
 up_rec(1)->engine=ENGI_SAMPLE;static up_rec_t legacy;legacy=*up_rec(1);
 REQUIRE(up_used(1) && up_playable_count()==2 && up_playable_nth(0)==0 && up_playable_nth(1)==2);
 REQUIRE(up_load(1)!=0 && up_put(1,0)!=0 && up_rename(1,"NO")!=0 && !memcmp(&legacy,up_rec(1),sizeof legacy));
 int16_t before[P_COUNT];memcpy(before,TSEL->p,sizeof before);
 unsigned total;for(unsigned n=0; n<(preset_all_pos(&total),total);n++){uint32_t k,e=preset_all_at(n,&k);REQUIRE(e==ENGI_GRAIN || (e==NENGINES && up_rec(k)->engine==ENGI_GRAIN));}
 for(unsigned i=0;i<64;i++){preset_step(1);REQUIRE(TSEL->eng_req==ENGI_GRAIN);midi_pc_load(TSEL,i);REQUIRE(TSEL->eng_req==ENGI_GRAIN);}
 ui_power_on();hold(B_EDIT);for(unsigned i=0;i<16;i++){edit_load(eng_step(ENGI_GRAIN,i),0);REQUIRE(TSEL->eng_req==ENGI_GRAIN);}
 printf("GRAIN-only: %u checks passed; selection, initialization, sources, personalities and legacy read-only policy\n",checks);return 0;
}
