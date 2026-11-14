/* SPDX-License-Identifier: GPL-3.0-only */
#define COFFEE_HOME_VISUALS 1
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"GRAIN theme %u: %s\n",__LINE__,#x);return 1;}}while(0)
static void footer(void){ui.help_t=0;ui.ml.on=0;ui.force=1;draw_foot();ui.force=0;}
static uint16_t pixel(unsigned i,unsigned x,unsigned y){return host_screen[(Y_FOOT+y)*240+(unsigned)bar_x(i)+x];}
int main(void){
 static uint16_t home[240*16],body[240*Y_FOOT];
 ui_power_on();kb_boot_hold=0;
 REQUIRE(swap16(HOME_SPRITE_PALETTE[189])==GRAIN_COFFEE);
 for(unsigned t=0;t<4;t++){
  track_t *p=&trk[t];memset(p->step,0,sizeof p->step);p->p[P_SLEN]=16;
  p->step[t].n=1;p->step[t].note[0]=60+t;
  p->step[4].hit=1;p->step[5].time=ST_TIE;
  p->step[6].time=ST_REST;p->step[6].n=p->step[6].hit=1;
  p->step[7].n=1;p->step[7].note[0]=0;p->step[7].flags=SF_ACCENT;
 }
 for(unsigned ch=0;ch<4;ch++)for(unsigned page=0;page<2;page++)for(unsigned playing=0;playing<2;playing++)for(unsigned ph=0;ph<16;ph++){
  song.sel=ch;song.playing=playing;TSEL->seq_idx=ph;go_home();footer();memcpy(home,host_screen+Y_FOOT*240,sizeof home);
  go_title(page?"EDIT 2":"EDIT 1");frame();memcpy(body,host_screen,sizeof body);footer();
  REQUIRE(!memcmp(home,host_screen+Y_FOOT*240,sizeof home));
  REQUIRE(!memcmp(body,host_screen,sizeof body));
  REQUIRE(pixel(ph,4,14)==swap16(playing?T_TEXT:T_BG));
  for(unsigned i=0;i<16;i++){
   int note=i==ch || i==4 || i==7;
   REQUIRE((pixel(i,4,1)==swap16(home_step_accent(ch)))==note);
   if(!note)REQUIRE(pixel(i,4,10)==swap16(T_MID));
  }
 }
 /* Real transport advances/reset and invalid banks, on both pages. */
 song.playing=0;ui_power_on();press(B_EDIT);song.rec=0;seq_start();seq_tick(TSEL,CTL);footer();
 REQUIRE(pixel(TSEL->seq_idx&15u,4,14)==swap16(T_TEXT));
 unsigned old=TSEL->seq_idx;events_block(div_samples(TSEL->p[P_SDIV])+CTL);footer();
 REQUIRE(TSEL->seq_idx!=old && pixel(TSEL->seq_idx&15u,4,14)==swap16(T_TEXT));
 seq_stop();footer();for(unsigned i=0;i<16;i++)REQUIRE(pixel(i,4,14)==swap16(T_BG));
 seq_start();seq_tick(TSEL,CTL);footer();REQUIRE(TSEL->seq_idx==0 && pixel(0,4,14)==swap16(T_TEXT));seq_stop();
 ui.bank=255;footer();for(unsigned i=0;i<16;i++)REQUIRE(pixel(i,4,1)==swap16(T_BG));
 /* Sharing preserves HOME's SONG data source instead of reading local notes. */
 ui.bank=0;chain.running=1;chain.lane[0]=1;memset(chain.source[1].step[0],0,sizeof chain.source[1].step[0]);
 chain.source[1].step[0][9].n=1;chain.source[1].step[0][9].note[0]=72;
 go_home();footer();memcpy(home,host_screen+Y_FOOT*240,sizeof home);go_title("EDIT 2");footer();REQUIRE(!memcmp(home,host_screen+Y_FOOT*240,sizeof home));
 chain.running=0;
 printf("GRAIN theme: %u checks passed; shared HOME symbols, channel patterns, playhead, transport and bounds\n",checks);return 0;
}
