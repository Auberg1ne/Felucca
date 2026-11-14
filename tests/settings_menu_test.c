/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"settings menu %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
 static const uint8_t rows[5][8]={
 {MI_ANIM,MI_LEDS,MI_SCROFF},
 {MI_HOLD,MI_ACCEL,MI_LATCH,MI_BPMLOCK,MI_SCLLED,MI_PREVIEW},
 {MI_LOWCUT,MI_USB,MI_CLICK,MI_CLKLVL,MI_COUNTIN,MI_TUNE},
 {MI_MIDIIN,MI_TCH1,MI_TCH2,MI_TCH3,MI_TCH4,MI_LCLEAR},
 {MI_SERIAL,MI_RESTORE,MI_HELP,
#if FELUCCA_CPU2
 MI_CPU2,
#endif
 MI_PANEL,MI_INFO,MI_ABOUT}};
 static const unsigned counts[]={3,6,6,6,6+FELUCCA_CPU2};
 static const unsigned hidden[]={MI_COLOR,MI_STYLE,MI_LARGE,MI_SCOPE,MI_HOME,MI_CHADD,MI_SELECT,MI_MFOLLOW,MI_SCRWAKE};
 ui_power_on();hold(B_HOME);REQUIRE(ui.menu_sel==MI_ANIM);
 for(unsigned tab=0;tab<5;tab++) {
  REQUIRE(menu_visible_count(tab)==counts[tab]);
  for(unsigned i=0;i<counts[tab];i++){REQUIRE(menu_visible_row(tab,i)==rows[tab][i]);REQUIRE(menu_visible_index(rows[tab][i])==i);}
  ui.menu_sel=rows[tab][0];menu_move(-1);REQUIRE(ui.menu_sel==rows[tab][counts[tab]-1]);menu_move(1);REQUIRE(ui.menu_sel==rows[tab][0]);
  for(unsigned i=1;i<counts[tab];i++){menu_move(1);REQUIRE(ui.menu_sel==rows[tab][i]);}
  menu_tab_go(1);menu_tab_go(-1);REQUIRE(ui.menu_sel==rows[tab][counts[tab]-1]);
 }
 for(unsigned i=0;i<NELEM(hidden);i++)menu_put(hidden[i],menu_n(hidden[i])-1);
 for(unsigned i=0;i<NELEM(hidden);i++) {
  unsigned before[NELEM(hidden)];for(unsigned j=0;j<NELEM(hidden);j++)before[j]=menu_get(hidden[j]);
  REQUIRE(!menu_visible(hidden[i]));
  ui.menu_row[MI_TAB[hidden[i]]]=counts[MI_TAB[hidden[i]]]-1;ui.menu_sel=hidden[i];menu_normalize();
  REQUIRE(ui.menu_sel==rows[MI_TAB[hidden[i]]][0]);
  memset(ui.menu_row,255,sizeof ui.menu_row);ui.menu_sel=hidden[i];ui.force=1;frame();
  REQUIRE(ui.menu_sel==rows[MI_TAB[hidden[i]]][0]);
  ui.menu_sel=hidden[i];turn(EN_K1,1);REQUIRE(menu_visible(ui.menu_sel));
  for(unsigned j=0;j<NELEM(hidden);j++)REQUIRE(menu_get(hidden[j])==before[j]);
 }
 ui.menu_sel=255;menu_input(0);REQUIRE(ui.menu_sel==MI_ANIM);
 /* Export/import must preserve nondefault hidden settings and unknown bytes. */
 persist_t p={0},q={0};ui_home_view=9;favorites.factory[15][13]=0xa5;settings_export(&p);
 memset(&favorites,0,sizeof favorites);REQUIRE(settings_import(&p,sizeof p)==1);settings_export(&q);REQUIRE(!memcmp(&p,&q,sizeof p));
 REQUIRE(ui_home_view==9 && favorites.factory[15][13]==0xa5);
 menu_put(MI_HOME,HV_LEVELS);for(unsigned k=0;k<4;k++){int16_t*vp;REQUIRE(home_param(k,&vp)==track_desc(&trk[k],P_LEVEL) && vp==&trk[k].p[P_LEVEL]);}
 REQUIRE(menu_visible(MI_SCLLED) && menu_visible(MI_HELP) && menu_visible(MI_PANEL) && menu_visible(MI_INFO) && menu_visible(MI_ABOUT));
 printf("settings visibility: %u checks passed, CPU2=%u, no added persistent state\n",checks,FELUCCA_CPU2);return 0;
}
