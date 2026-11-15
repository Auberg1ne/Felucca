/* SPDX-License-Identifier: GPL-3.0-only */
#define COFFEE_HOME_VISUALS 1
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"LFO MOD %u: %s\n",__LINE__,#x);return 1;}}while(0)
static int redraw(void){static uint16_t p[240*240];ui.help_t=ui.msg_t=ui.hot_t=ui.bpm_t=0;frame();memcpy(p,host_screen,sizeof p);ui.force=1;frame();REQUIRE(!memcmp(p+28*240,host_screen+28*240,(Y_FOOT-28)*480));return 0;}
int main(void){
 static const unsigned ids[3][4]={{P_LRATE,P_LWAVE,P_LPHASE,P_LFADE},{P_LSYNC,P_LTRIG,P_LPOL,255},{P_LD_PIT,P_LD_FLT,P_LD_SHP,P_LD_AMP}};
 ui_power_on();unsigned first=page_first(FAM_LFO),matrix=page_first(FAM_SCL);
 REQUIRE(lfo_grid_page(&PAGES[first]) && PAGES[matrix].graph==GR_MOD);
 for(unsigned t=0;t<4;t++)for(unsigned pref=0;pref<SEL_N;pref++){
  go_home();track_select(t);menu_put(MI_SELECT,pref);unsigned saved=ui_prefs2;press(B_LFO);frames(400);
  REQUIRE(ui.page==first && grain_edit_row==0);unsigned count=0;
  for(unsigned i=0;i<NPAGES;i++){if(page_visible(i)&&page_family(&PAGES[i])==FAM_LFO)count++;if(PAGES[i].fam==FAM_SCL)REQUIRE(!page_visible(i));}
  REQUIRE(count==1);
  for(unsigned r=0;r<3;r++){
   REQUIRE(grain_edit_row==r);
   for(unsigned c=0;c<4;c++){
    unsigned id=ids[r][c];REQUIRE(page_param_id(cur_page(),c)==id);
    int16_t before[P_COUNT];memcpy(before,TSEL->p,sizeof before);
    if(id==255){unsigned n=motion.count;turn(EN_K1+c,10);REQUIRE(!memcmp(before,TSEL->p,sizeof before)&&motion.count==n);continue;}
    const param_desc_t*d=track_desc(TSEL,id);TSEL->p[id]=d->min;turn(EN_K1+c,1);REQUIRE(TSEL->p[id]>d->min);
    for(unsigned k=0;k<P_COUNT;k++)if(k!=id)REQUIRE(TSEL->p[k]==before[k]);
    ui.ml.on=1;turn(EN_K1+c,1);REQUIRE(ui.ml.pick&&ui.ml.id==id&&ui.ml.trk==t);ui.ml.on=ui.ml.pick=0;
    if(redraw())return 1;
   }
   turn(EN_SELECT,1);
  }
  REQUIRE(grain_edit_row==0 && ui_prefs2==saved);turn(EN_SELECT,-1);REQUIRE(grain_edit_row==2);turn(EN_SELECT,1);REQUIRE(grain_edit_row==0);
  for(unsigned n=0;n<3;n++){press(B_LFO);REQUIRE(ui.page==first&&grain_edit_row==0);}
  ui_prefs|=PREF_BPM_LOCK;int bpm=song.g[G_BPM];btn_down(B_GLO);frame();turn(EN_SELECT,1);btn_up(B_GLO);frame();
  REQUIRE(song.g[G_BPM]>bpm && grain_edit_row==0);ui_prefs&=~PREF_BPM_LOCK;frames(400);
  press(B_SCL);REQUIRE(ui.page==matrix&&!ui.layer&&cur_fam()==FAM_SCL);int16_t scale[6];
  const unsigned hidden[]={P_ROOT,P_SCALE,P_QUANT,P_TRANS,P_CHRD,P_VOIC};for(unsigned k=0;k<6;k++)scale[k]=TSEL->p[hidden[k]];
  for(unsigned slot=0;slot<4;slot++){
   for(unsigned n=0;n<4;n++)turn(EN_K1,-1);for(unsigned n=0;n<slot;n++)turn(EN_K1,1);REQUIRE(mod_ui_slot==slot);
   for(unsigned c=1;c<4;c++){
    unsigned id=P_M1SRC+3*slot+c-1;int16_t*p;REQUIRE(page_desc(cur_page(),c,&p)==track_desc(TSEL,id)&&p==&TSEL->p[id]);
    const param_desc_t*d=track_desc(TSEL,id);TSEL->p[id]=d->min;int16_t old[P_COUNT];memcpy(old,TSEL->p,sizeof old);turn(EN_K1+c,1);REQUIRE(TSEL->p[id]>d->min);
    for(unsigned k=0;k<P_COUNT;k++)if(k!=id)REQUIRE(TSEL->p[k]==old[k]);
    ui.ml.on=1;turn(EN_K1+c,1);REQUIRE(ui.ml.id==id);ui.ml.on=ui.ml.pick=0;if(redraw())return 1;
   }
  }
  /* Repeat taps, long holds and old combo entrances never open the SCL layer. */
  press(B_SCL);press(B_SCL);REQUIRE(ui.page==matrix&&!ui.layer&&!ui.lock);
  hold(B_SCL);REQUIRE(ui.page==matrix&&!ui.layer&&!ui.lock);
  btn_down(B_SCL);frame();tap_key(7);REQUIRE(!ui.layer && !ui.lock);btn_up(B_SCL);frame();
  btn_down(B_SCL);frame();turn(EN_SELECT,1);btn_down(B_OCTUP);frame();btn_up(B_OCTUP);frame();btn_up(B_SCL);frame();REQUIRE(ui.page==matrix&&!ui.layer);
  for(unsigned k=0;k<6;k++)REQUIRE(TSEL->p[hidden[k]]==scale[k]);
  for(unsigned i=0;i<NPAGES;i++)if(PAGES[i].fam==FAM_SCL || (PAGES[i].fam==FAM_LFO&&PAGES[i].graph!=GR_MOD&&i!=first)){
   ui.page=i;ui.home=0;frame();REQUIRE(ui.page==(PAGES[i].fam==FAM_SCL?matrix:first));
  }
  go_home();REQUIRE(ui_prefs2==saved&&sel_mode()==pref);
 }
 /* Remembered locks retain the selected LFO row; empty/sync flags retain eligibility. */
 go_home();press(B_LFO);turn(EN_SELECT,2);unsigned src=ui.plk_src;press(B_SEQ);REQUIRE(ui.plk_src==src&&lock_id(0)==P_LD_PIT&&lock_id(3)==P_LD_AMP);
 TSEL->p[P_LD_PIT]=0;lock_turn(0,10,1ull<<2);int16_t v;REQUIRE(motion_lock_get(TSEL,2,P_LD_PIT,&v)&&v==10);motion_clear(TSEL);
 go_home();press(B_LFO);turn(EN_SELECT,2);song.rec=1u<<song.sel;seq_start();seq_tick(TSEL,CTL);TSEL->p[P_LD_AMP]=0;turn(EN_K4,10);
 REQUIRE(motion.count==1&&motion.event[0].param==P_LD_AMP);seq_stop();motion_clear(TSEL);
 go_home();press(B_LFO);turn(EN_SELECT,1);src=ui.plk_src;press(B_SEQ);REQUIRE(ui.plk_src==src&&!motion_param(P_LSYNC));for(unsigned c=0;c<4;c++)REQUIRE(lock_id(c)==255);
 go_home();press(B_LFO);turn(EN_SELECT,2);int original=TSEL->p[P_LD_PIT];
 btn_down(B_LFO);frame();turn(EN_K1,1);REQUIRE(TSEL->p[P_LD_PIT]==original+1);btn_up(B_LFO);frame();REQUIRE(TSEL->p[P_LD_PIT]==original);
 static project_t p,q;static project_store_t store;project_capture(&p);REQUIRE(proj_pack(&store,&p)&&proj_import(&q,&store,sizeof store));
 REQUIRE(!project_restore_runtime(&q));for(unsigned t=0;t<4;t++)for(unsigned i=0;i<12;i++)REQUIRE(trk[t].p[P_M1SRC+i]==p.t[t].p[P_M1SRC+i]);
 int rc=up_store(0,"LFO MOD");REQUIRE(rc==0||rc==3);int16_t before[P_COUNT];memcpy(before,TSEL->p,sizeof before);apply_preset(1);REQUIRE(!up_load(0));
 for(unsigned r=0;r<3;r++)for(unsigned c=0;c<4;c++)if(ids[r][c]<P_COUNT)REQUIRE(TSEL->p[ids[r][c]]==before[ids[r][c]]);
 for(unsigned i=P_M1SRC;i<=P_M4AMT;i++)REQUIRE(TSEL->p[i]==before[i]);
 undo.keep=0;memcpy(before,TSEL->p,sizeof before);apply_preset(2);undo_swap();REQUIRE(!memcmp(before,TSEL->p,sizeof before));
 /* These buttons share a switch group with SCL; a consumed hold must not tap a page. */
 const unsigned other[]={B_FX,B_GLO,B_SEQ,B_SAVE};
 for(unsigned i=0;i<NELEM(other);i++){go_home();frames(400);hold(other[i]);REQUIRE(ui.home && !ui.layer && !ui.lock);}
 printf("LFO MOD UI: %u checks passed\n",checks);return 0;
}
