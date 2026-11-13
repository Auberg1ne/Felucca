/* SPDX-License-Identifier: GPL-3.0-only */
#define MIXER_VISUALS_NO_MAIN 1
#include "mixer_visuals_test.c"
int main(void)
{
 web_nor_erase();web_boot();while(fm1_ms<3500)web_ms();home_tap();ui_draw();
 for(unsigned c=0;c<4;c++)for(unsigned k=0;k<4;k++) {
  song.sel=c;unsigned id=MIXER_TOP_ID[k];
  for(int v=TP[id].min;v<=TP[id].max;v++) {
   trk[c].p[id]=v;ui.force=1;captured_n=0;ui_draw();
   char val[16],text[24];const char *unit;param_format(&TP[id],v,val,&unit);
   snprintf(text,sizeof text,"%s%s%s",val,*unit?" ":"",unit);REQUIRE(seen(text));
   for(unsigned j=0;j<3;j++)REQUIRE(web_fb[(Y_LABEL+21)*240+CARD_X(j)+28]==swap16(HOME_ACCENT[c]));
   unsigned center=web_fb[(Y_LABEL+21)*240+CARD_X(3)+28];
   REQUIRE(center==(trk[c].p[P_MUTE]?swap16(HOME_ACCENT[c]):0));
   REQUIRE(web_fb[(Y_LABEL+14)*240+CARD_X(3)+28]==swap16(HOME_ACCENT[c]));
  }
 }
 REQUIRE(!bad_rects && !value_clips);
 printf("MIXER top knobs: %u checks passed; cache %zu B; bitmap bytes 0\n",checks,sizeof mixer_top);return 0;
}
