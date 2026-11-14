/* SPDX-License-Identifier: GPL-3.0-only */
#define MIXER_VISUALS_NO_MAIN 1
#include "mixer_visuals_test.c"
static void tick(unsigned ms){fm1_ms+=ms;captured_n=0;ui_draw();}
static uint32_t response(unsigned cadence)
{
 mixer_top.valid=0;trk[song.sel].p[P_REV]=0;tick(0);
 trk[song.sel].p[P_REV]=127;
 for(unsigned n=0;n<240;n+=cadence)tick(cadence);
 return mixer_top.position[2];
}
int main(void)
{
 web_nor_erase();web_boot();while(fm1_ms<3500)web_ms();home_tap();ui_draw();
 REQUIRE(mixer_visuals_page());
 trk[0].p[P_REV]=0;song.sel=0;mixer_top.valid=0;tick(0);
 trk[0].p[P_REV]=127;tick(0);REQUIRE(seen("100 %") && mixer_top.position[2]==0);
 tick(16);REQUIRE(mixer_top.position[2]>0 && mixer_top.position[2]<32768);
 unsigned rising=mixer_top.position[2];trk[0].p[P_REV]=0;tick(16);
 REQUIRE(mixer_top.position[2]<rising && mixer_top.position[2]>0 && seen("0 %"));
 for(unsigned n=0;n<100;n++)tick(16);REQUIRE(mixer_top.position[2]==0);
 uint32_t a=response(10),b=response(20),c=response(30);
 REQUIRE(abs((int)a-(int)b)<40 && abs((int)a-(int)c)<40);
 REQUIRE(a>31000 && a<31300); /* exp(-240/80): about 95% settled */
 trk[0].p[P_REV]=0;uint32_t before=mixer_top.position[2];tick(5000);
 REQUIRE(mixer_top.position[2]>before/2 && mixer_top.position[2]<before); /* capped dt */
 mixer_top.ms=UINT32_MAX-7;fm1_ms=8;ui_draw();REQUIRE(mixer_top.position[2]<before);
 for(unsigned n=1;n<=4;n++) {
  unsigned ch=n&3u;song.sel=ch;trk[ch].p[P_LEVEL]=ch*30;trk[ch].p[P_PAN]=-64+ch*40;trk[ch].p[P_REV]=ch*40;tick(0);
  for(unsigned k=0;k<3;k++){unsigned id=MIXER_TOP_ID[k];REQUIRE(mixer_top.position[k]==(uint32_t)(trk[ch].p[id]-TP[id].min)*32768u/(TP[id].max-TP[id].min));}
  trk[ch].p[P_MUTE]=1;tick(0);REQUIRE(seen("ON"));trk[ch].p[P_MUTE]=0;tick(0);REQUIRE(seen("OFF"));
 }
 home_tap();ui_draw();uint32_t held=mixer_top.position[0];tick(100);REQUIRE(!mixer_top.valid && held==mixer_top.position[0]);
 home_tap();ui_draw();home_tap();ui_draw();REQUIRE(mixer_visuals_page() && mixer_top.valid);
 REQUIRE(!bad_rects && !value_clips);
 printf("MIXER smoothing: %u checks passed; cache %zu B\n",checks,sizeof mixer_top);return 0;
}
