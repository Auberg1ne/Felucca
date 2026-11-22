/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define REQUIRE(x) do { checks++; if(!(x)){fprintf(stderr,"grid %u: %s\n",__LINE__,#x);return 1;} }while(0)
int main(void)
{
 static const unsigned expected[2][3][4]={{{110,111,112,113},{114,115,116,117},{108,109,255,255}},{{103,104,105,118},{106,107,255,255},{37,44,95,255}}};
 static const unsigned hidden[]={38,41,42,43,39,40};
 ui_power_on();press(B_EDIT);REQUIRE(grain_grid_active() && grain_grid_page(cur_page())==1 && grain_edit_row==0);
 unsigned visible=0;for(unsigned i=0;i<NPAGES;i++)if(PAGES[i].fam==FAM_EDIT && page_visible(i))visible++;
 REQUIRE(visible==2);
 for(unsigned page=0;page<2;page++) {
  REQUIRE(grain_grid_page(cur_page())==page+1);
  for(unsigned row=0;row<3;row++) {
   REQUIRE(grain_edit_row==row);
   for(unsigned col=0;col<4;col++) {
    unsigned id=expected[page][row][col];int16_t *vp=NULL;
    REQUIRE(grain_grid_param(page,row,col)==id && page_param_id(cur_page(),col)==id);
    const param_desc_t *d=page_desc(cur_page(),col,&vp);
    if(id==255){REQUIRE(d==NULL);int16_t before[P_COUNT];memcpy(before,TSEL->p,sizeof before);ui.ml.on=1;ui.ml.pick=0;turn(EN_K1+col,7);REQUIRE(!memcmp(before,TSEL->p,sizeof before) && !ui.ml.pick);ui.ml.on=0;continue;}
    REQUIRE(d==track_desc(TSEL,id) && vp==&TSEL->p[id]);
    TSEL->p[id]=d->min;int16_t before[P_COUNT];memcpy(before,TSEL->p,sizeof before);
    int target=param_turn(d,d->min,1);ui.ml.on=1;ui.ml.pick=0;
    turn(EN_K1+col,1);REQUIRE(TSEL->p[id]==target && ((id==P_GWIN||id==P_GRC)?!ui.ml.pick:(ui.ml.pick && ui.ml.id==id)));ui.ml.on=0;
    for(unsigned j=0;j<P_COUNT;j++)if(j!=id)REQUIRE(TSEL->p[j]==before[j]);
    REQUIRE(lock_id(col)==(motion_param(id)?id:255u));
   }
   turn(EN_SELECT,1);
  }
  REQUIRE(grain_edit_row==0);turn(EN_SELECT,-1);REQUIRE(grain_edit_row==2);
  turn(EN_SELECT,4);REQUIRE(grain_edit_row==0);turn(EN_SELECT,-4);REQUIRE(grain_edit_row==2);
  press(B_EDIT);REQUIRE(grain_edit_row==0);
 }
 REQUIRE(grain_grid_page(cur_page())==1);
 /* SELECT preference is a temporary override, not a preference rewrite. */
 for(unsigned mode=0;mode<SEL_N;mode++) {
  menu_put(MI_SELECT,mode);unsigned prefs=ui_prefs2;int bpm=song.g[G_BPM];unsigned page=ui.page;
  turn(EN_SELECT,1);REQUIRE(grain_edit_row==1 && ui.page==page && song.g[G_BPM]==bpm && ui_prefs2==prefs);
  turn(EN_SELECT,-1);REQUIRE(grain_edit_row==0 && sel_mode()==mode);
 }
 menu_put(MI_SELECT,SEL_BPM);ui_prefs|=PREF_BPM_LOCK;int bpm=song.g[G_BPM];
 turn(EN_SELECT,1);REQUIRE(grain_edit_row==1 && song.g[G_BPM]==bpm);turn(EN_SELECT,-1);REQUIRE(grain_edit_row==0);
 btn_down(B_GLO);frame();turn(EN_SELECT,1);btn_up(B_GLO);frame();frames(320);
 REQUIRE(song.g[G_BPM]>bpm && grain_edit_row==0 && (ui_prefs&PREF_BPM_LOCK));
 go_home();bpm=song.g[G_BPM];turn(EN_SELECT,1);REQUIRE(song.g[G_BPM]==bpm);ui_prefs&=~PREF_BPM_LOCK;
 turn(EN_SELECT,1);REQUIRE(song.g[G_BPM]>bpm && sel_mode()==SEL_BPM);
 press(B_EDIT);turn(EN_SELECT,2);REQUIRE(grain_edit_row==2);
 /* A SEQ lock still addresses the last selected GRAIN row. */
 unsigned source=ui.plk_src;press(B_SEQ);REQUIRE(ui.plk_src==source && lock_id(0)==108 && lock_id(1)==255);
 TSEL->p[108]=0;lock_turn(0,64,1ull<<3);int16_t locked;REQUIRE(motion_lock_get(TSEL,3,108,&locked) && locked==64);
 unsigned events=motion.count;lock_turn(1,1,1ull<<3);REQUIRE(motion.count==events);motion_clear(TSEL);
 go_home();press(B_EDIT);REQUIRE(grain_edit_row==0);turn(EN_SELECT,2);TSEL->p[108]=0;turn(EN_K1,64);REQUIRE(TSEL->p[108]==64);
 static project_t p,q;static project_store_t stored;project_capture(&p);REQUIRE(proj_pack(&stored,&p) && proj_import(&q,&stored,sizeof stored));
 TSEL->p[108]=0;REQUIRE(!project_restore_runtime(&q) && TSEL->p[108]==64);
 go_title("EDIT 1");
 int16_t h[NELEM(hidden)];for(unsigned i=0;i<NELEM(hidden);i++)h[i]=TSEL->p[hidden[i]]=track_desc(TSEL,hidden[i])->max;
 frames(400);press(B_EDIT);for(unsigned r=0;r<3;r++){for(unsigned c=0;c<4;c++)turn(EN_K1+c,1);turn(EN_SELECT,1);}
 for(unsigned i=0;i<NELEM(hidden);i++)REQUIRE(TSEL->p[hidden[i]]==h[i]);
 go_title("EDIT 2");turn(EN_SELECT,2);set_engine_of(&trk[1],ENGI_GRAIN);track_select(1);REQUIRE(grain_edit_row==0);
 /* Other engines retain their descriptor layout and normal SELECT PAGE navigation. */
 ui_power_on();set_engine_of(TSEL,ENGI_DRUM);TSEL->engine=ENGI_DRUM;menu_put(MI_SELECT,SEL_PAGE);go_title("EDIT 1");
 REQUIRE(!grain_grid_active());unsigned old=ui.page;turn(EN_SELECT,1);REQUIRE(ui.page!=old && sel_mode()==SEL_PAGE);
 for(unsigned i=0;i<NPAGES;i++)for(unsigned c=0;c<4;c++)REQUIRE(page_param_id(&PAGES[i],c)==PAGES[i].id[c]);
 /* Actual knob automation and momentary edits use the active row, not old EDIT IDs. */
 ui_power_on();press(B_EDIT);turn(EN_SELECT,2);song.rec=1;seq_start();seq_tick(TSEL,CTL);
 TSEL->p[108]=0;turn(EN_K1,64);REQUIRE(motion.count==1 && motion.event[0].param==108 && motion.event[0].value==64);
 turn(EN_K2,1);REQUIRE(motion.count==1);seq_stop();
 ui_power_on();press(B_EDIT);turn(EN_SELECT,1);int original=TSEL->p[P_E5];
 btn_down(B_LFO);frame();turn(EN_SELECT,1);REQUIRE(grain_edit_row==1);turn(EN_K2,1);REQUIRE(TSEL->p[P_E5]==original+1 && grain_edit_row==1);
 btn_up(B_LFO);frame();REQUIRE(TSEL->p[P_E5]==original && !motion.count);
 for(unsigned mode=SEL_MODW;mode<=SEL_VEL;mode++) {
  ui_power_on();TSEL->mw=0;kb_vel=100;menu_put(MI_SELECT,mode);unsigned saved=ui_prefs2;press(B_EDIT);turn(EN_SELECT,1);
  REQUIRE(grain_edit_row==1 && ui_prefs2==saved && TSEL->mw==0 && kb_vel==100);go_home();turn(EN_SELECT,1);REQUIRE(sel_mode()==mode && ui_prefs2==saved);
  REQUIRE(mode==SEL_MODW?(TSEL->mw>0 && kb_vel==100):(kb_vel>100 && !TSEL->mw));
 }
 REQUIRE(grain_grid_param(2,0,0)==255 && grain_grid_param(0,3,0)==255 && grain_grid_param(0,0,4)==255);
 printf("GRAIN grid: %u checks passed\n",checks);return 0;
}
