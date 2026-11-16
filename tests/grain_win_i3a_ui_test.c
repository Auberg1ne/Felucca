/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"WIN UI %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 const unsigned want[2][3][4]={{{110,111,112,113},{114,115,116,117},{108,109,255,255}},{{103,104,105,255},{106,107,255,255},{37,44,95,255}}};
 for(unsigned p=0;p<2;p++)for(unsigned r=0;r<3;r++)for(unsigned c=0;c<4;c++)CHECK(grain_grid_param(p,r,c)==want[p][r][c]);
 ui_power_on();press(B_EDIT);
 for(unsigned track=0;track<4;track++){
  track_select(track);CHECK(grain_edit_row==0);turn(EN_SELECT,2);CHECK(grain_edit_row==2 && page_param_id(cur_page(),1)==109);
  int16_t other[4];for(unsigned t=0;t<4;t++)other[t]=trk[t].p[109];
  for(unsigned value=0;value<=127;value++){
   TSEL->p[109]=value?value-1:0;int16_t before[P_COUNT];memcpy(before,TSEL->p,sizeof before);ui.ml.on=1;ui.ml.pick=0;
   turn(EN_K2,value?1:-1);CHECK(TSEL->p[109]==value && !ui.ml.pick && !motion.count);ui.ml.on=0;
   for(unsigned id=0;id<P_COUNT;id++)if(id!=109)CHECK(TSEL->p[id]==before[id]);
   for(unsigned t=0;t<4;t++)if(t!=track)CHECK(trk[t].p[109]==other[t]);
  }
  TSEL->p[109]=0;turn(EN_K2,-8);CHECK(!TSEL->p[109]);TSEL->p[109]=127;turn(EN_K2,8);CHECK(TSEL->p[109]==127);
  unsigned count=motion.count;CHECK(lock_id(1)==255 && ml_learn(70,track,109)==2);lock_turn(1,1,1ull<<3);CHECK(motion.count==count);
  TSEL->p[109]=64;btn_down(B_LFO);frame();turn(EN_K2,1);CHECK(TSEL->p[109]==65);btn_up(B_LFO);frame();CHECK(TSEL->p[109]==64);
 }
 for(unsigned mode=0;mode<SEL_N;mode++){
  go_home();menu_put(MI_SELECT,mode);unsigned saved=ui_prefs2;frames(400);press(B_EDIT);CHECK(grain_grid_active());turn(EN_SELECT,-1);CHECK(grain_edit_row==2 && ui_prefs2==saved);turn(EN_SELECT,1);CHECK(grain_edit_row==0);
 }
 turn(EN_SELECT,2);int bpm=song.g[G_BPM];btn_down(B_GLO);frame();turn(EN_SELECT,1);btn_up(B_GLO);frame();frames(320);CHECK(song.g[G_BPM]>bpm && grain_edit_row==2);
 press(B_EDIT);CHECK(grain_grid_page(cur_page())==2 && grain_edit_row==0);frames(400);press(B_EDIT);CHECK(grain_grid_page(cur_page())==1 && grain_edit_row==0);
 CHECK(!motion_param(109));CHECK(sizeof(gr_grain_t)==64 && sizeof(gr_part_t)==7296 && sizeof(track_t)==1840);
 printf("WIN device UI: %u checks passed; full values/four tracks, mapping, modifiers and no learn/automation\n",checks);return 0;
}
