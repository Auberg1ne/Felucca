/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"SAMPLE grid %u: %s (page=%s row=%u)\n",__LINE__,#x,cur_page()->title,edit_grid_row);return 1;}}while(0)
static void sample_boot(void){ui_power_on();set_engine_of(TSEL,ENGI_SAMPLE);TSEL->engine=ENGI_SAMPLE;press(B_EDIT);}
int main(void){
 static const unsigned map[2][4]={{112,113,114,255},{37,44,95,255}};
 static const unsigned hidden[]={110,111,116,38,41,42,43,39,40,115,117};
 sample_boot();REQUIRE(sample_grid_page(cur_page()) && edit_grid_row==0 && edit_grid_active());
 unsigned page=ui.page,visible=0;for(unsigned i=0;i<NPAGES;i++)visible+=PAGES[i].fam==FAM_EDIT && page_visible(i);REQUIRE(visible==1);
 for(unsigned r=0;r<3;r++)for(unsigned c=0;c<4;c++)REQUIRE(edit_grid_param(3,r,c)==(r<2?map[r][c]:255));
 REQUIRE(edit_grid_param(3,0,4)==255);
 for(unsigned r=0;r<2;r++){
  REQUIRE(edit_grid_row==r);
  for(unsigned c=0;c<4;c++){
   int16_t *vp=NULL;const param_desc_t*d=page_desc(cur_page(),c,&vp);unsigned id=map[r][c];
   REQUIRE(page_param_id(cur_page(),c)==id);
   if(id!=255){REQUIRE(d==track_desc(TSEL,id) && vp==&TSEL->p[id]);TSEL->p[id]=d->min;}
   else REQUIRE(!d && !vp);
   int16_t before[P_COUNT];memcpy(before,TSEL->p,sizeof before);ui.ml.on=1;ui.ml.pick=0;turn(EN_K1+c,1);
   REQUIRE(id==255?(!ui.ml.pick && !memcmp(before,TSEL->p,sizeof before)):(TSEL->p[id]==param_turn(d,before[id],1) && ui.ml.pick && ui.ml.id==id));ui.ml.on=0;
   for(unsigned i=0;i<P_COUNT;i++)if(i!=id)REQUIRE(TSEL->p[i]==before[i]);
   REQUIRE(lock_id(c)==(id!=255 && motion_param(id)?id:255u));
  }
  turn(EN_SELECT,1);
 }
 REQUIRE(!edit_grid_row);turn(EN_SELECT,-1);REQUIRE(edit_grid_row==1);turn(EN_SELECT,-1);REQUIRE(!edit_grid_row);
 turn(EN_SELECT,5);REQUIRE(edit_grid_row==1);turn(EN_SELECT,-5);REQUIRE(!edit_grid_row);
 for(unsigned mode=0;mode<SEL_N;mode++){
  sample_boot();menu_put(MI_SELECT,mode);unsigned prefs=ui_prefs2;int bpm=song.g[G_BPM];TSEL->mw=0;kb_vel=100;
  turn(EN_SELECT,1);REQUIRE(edit_grid_row==1 && ui.page==page && ui_prefs2==prefs && song.g[G_BPM]==bpm && !TSEL->mw && kb_vel==100);
  turn(EN_SELECT,-1);REQUIRE(!edit_grid_row);turn(EN_SELECT,-1);REQUIRE(edit_grid_row==1);
  go_home();turn(EN_SELECT,1);REQUIRE(ui_prefs2==prefs);
  if(mode==SEL_BPM)REQUIRE(song.g[G_BPM]>bpm);
  if(mode==SEL_PAGE)REQUIRE(!ui.home && cur_fam()==FAM_TRK);
  if(mode==SEL_MODW)REQUIRE(TSEL->mw>0);
  if(mode==SEL_VEL)REQUIRE(kb_vel>100);
 }
 sample_boot();ui_prefs|=PREF_BPM_LOCK;int bpm=song.g[G_BPM];turn(EN_SELECT,1);REQUIRE(edit_grid_row==1 && song.g[G_BPM]==bpm);
 btn_down(B_GLO);frame();turn(EN_SELECT,1);REQUIRE(song.g[G_BPM]>bpm && edit_grid_row==1);btn_up(B_GLO);frames(400);
 for(unsigned i=0;i<8;i++){press(B_EDIT);REQUIRE(ui.page==page && !edit_grid_row);frames(400);}
 turn(EN_SELECT,1);set_engine_of(&trk[1],ENGI_SAMPLE);trk[1].engine=ENGI_SAMPLE;turn(EN_ALGO,1);REQUIRE(song.sel==1 && !edit_grid_row && sample_grid_page(cur_page()));
 /* Hidden/stale raw page indices normalize before the first physical knob edit. */
 for(unsigned i=0;i<NPAGES;i++)if(PAGES[i].fam==FAM_EDIT && i!=page){
  sample_boot();int16_t before[P_COUNT];memcpy(before,TSEL->p,sizeof before);ui.home=0;ui.page=i;ui.plk_src=i+1;edit_grid_row=2;
  turn(EN_K4,1);REQUIRE(ui.page==page && !edit_grid_row && ui.plk_src==page+1 && !memcmp(before,TSEL->p,sizeof before));
 }
 /* Both remembered rows resolve on SEQ; K4 never aliases a real parameter. */
 for(unsigned r=0;r<2;r++)for(unsigned c=0;c<4;c++){
  sample_boot();if(r)turn(EN_SELECT,1);unsigned id=map[r][c];
  if(id!=255)TSEL->p[id]=track_desc(TSEL,id)->min;
  press(B_SEQ);REQUIRE(ui.plk_src==page+1 && edit_grid_row==r && lock_id(c)==(id!=255 && motion_param(id)?id:255u));
  lock_turn(c,1,1ull<<3);REQUIRE(motion.count==(id!=255 && motion_param(id)?1u:0u));
  if(motion.count)REQUIRE(MOTION_ID(&motion.event[0])==id);
 }
 for(unsigned r=0;r<2;r++)for(unsigned c=0;c<4;c++){
  sample_boot();if(r)turn(EN_SELECT,1);unsigned id=map[r][c];if(id!=255)TSEL->p[id]=track_desc(TSEL,id)->min;
  song.rec=1;seq_start();seq_tick(TSEL,CTL);ui.ml.on=1;ui.ml.pick=0;turn(EN_K1+c,1);
  REQUIRE(motion.count==(id!=255 && motion_param(id)?1u:0u));
  if(motion.count)REQUIRE(MOTION_ID(&motion.event[0])==id);
  REQUIRE(id==255?!ui.ml.pick:(ui.ml.pick && ui.ml.id==id));seq_stop();
 }
 sample_boot();turn(EN_SELECT,1);int old=TSEL->p[P_DETUNE];btn_down(B_LFO);frame();turn(EN_K2,1);REQUIRE(TSEL->p[P_DETUNE]==old+1 && edit_grid_row==1);
 turn(EN_K4,4);REQUIRE(mom.n==1);btn_up(B_LFO);frame();REQUIRE(TSEL->p[P_DETUNE]==old && !motion.count);
 /* Supported external control paths and saved data keep all hidden values. */
 sample_boot();for(unsigned i=0;i<NELEM(hidden);i++){
  unsigned id=hidden[i];const param_desc_t*d=track_desc(TSEL,id);midi_cc_set(TSEL,id,127);REQUIRE(TSEL->p[id]==d->max);
  REQUIRE(ml_learn(70,0,id)==1);midi_learned(70,0);REQUIRE(TSEL->p[id]==d->min);
 }
 TSEL->p[110]=14;TSEL->p[111]=7;TSEL->p[116]=73;TSEL->p[38]=45;TSEL->p[41]=1;TSEL->p[42]=2;TSEL->p[43]=1;TSEL->p[39]=-21;TSEL->p[40]=1;
 int16_t saved[P_COUNT];memcpy(saved,TSEL->p,sizeof saved);
 for(unsigned r=0;r<2;r++){for(unsigned c=0;c<4;c++)turn(EN_K1+c,1);turn(EN_SELECT,1);}
 for(unsigned i=0;i<NELEM(hidden);i++)REQUIRE(TSEL->p[hidden[i]]==saved[hidden[i]]);
 static project_t p,q;static project_store_t packed;project_capture(&p);REQUIRE(proj_pack(&packed,&p) && proj_import(&q,&packed,sizeof packed));
 REQUIRE(!project_restore_runtime(&q));for(unsigned i=0;i<NELEM(hidden);i++)REQUIRE(TSEL->p[hidden[i]]==saved[hidden[i]]);
 unsigned rc=up_store(0,"SAMPLE GRID");REQUIRE(rc==0 || rc==3);for(unsigned i=0;i<NELEM(hidden);i++)REQUIRE(up_value(up_rec(0),hidden[i])==saved[hidden[i]]);
 /* Preset load keeps the destination mix and does not turn source changes into UI edits. */
 int pan=TSEL->p[P_PAN],mute=TSEL->p[P_MUTE];apply_preset_to(TSEL,2);REQUIRE(TSEL->p[P_PAN]==pan && TSEL->p[P_MUTE]==mute);
 sample_boot();frames(400);btn_down(B_EDIT);frames(800);REQUIRE(ui.layer==LAYER_EDIT);btn_up(B_EDIT);frame(); /* engine-layer hold, not a hidden page */
 sample_boot();press(B_EDIT);REQUIRE(ui.layer==LAYER_EDIT && ui.lock==LAYER_EDIT); /* existing double tap */
 printf("SAMPLE grid: %u checks passed\n",checks);return 0;
}
