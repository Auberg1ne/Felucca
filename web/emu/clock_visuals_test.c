/* SPDX-License-Identifier: GPL-3.0-only */
#define MIXER_VISUALS_NO_MAIN 1
#include "mixer_visuals_test.c"
static void tick(unsigned ms){fm1_ms+=ms;captured_n=0;ui_draw();}
static int expected(unsigned id){char v[16],text[24];const char*u;param_format(&GP[id],song.g[id],v,&u);snprintf(text,sizeof text,"%s%s%s",v,*u?" ":"",u);return seen(text);}
int main(int argc,char **argv)
{
 web_nor_erase();web_boot();while(fm1_ms<3500)web_ms();home_tap();home_tap();ui_draw();
 REQUIRE(clock_visuals_page());
 for(unsigned k=0;k<3;k++)for(int v=GP[CLOCK_ID[k]].min;v<=GP[CLOCK_ID[k]].max;v++) {
  song.g[CLOCK_ID[k]]=v;ui.force=1;tick(16);REQUIRE(expected(CLOCK_ID[k]));
 }
 song.g[G_BPM]=40;clock_visuals.valid=0;tick(0);song.g[G_BPM]=240;tick(0);
 REQUIRE(clock_visuals.position[0]==0 && expected(G_BPM));tick(16);REQUIRE(clock_visuals.position[0]>0 && clock_visuals.position[0]<32768);
 for(unsigned n=0;n<100;n++)tick(16);REQUIRE(clock_visuals.position[0]==32768);
 for(unsigned src=0;src<3;src++) {
  song.g[G_CLOCK]=src;tick(0);REQUIRE(seen(N_CLOCK[src]));song.playing=1;
  for(unsigned i=0;i<16;i++){TSEL->seq_pos=0;TSEL->seq_idx=i;tick(15);REQUIRE((clock_visuals.beat&31)==i);
   if(!(i&3u)){static const unsigned corners[4][2]={{114,81},{162,129},{114,177},{66,129}};REQUIRE(web_fb[corners[i/4][1]*240+corners[i/4][0]]==swap16(0xffff));}
  }
  unsigned before=clock_step();tick(1000);REQUIRE(clock_step()==before); /* display time never advances a beat */
  song.playing=0;tick(0);REQUIRE(seen("STOP") && !(clock_visuals.beat&32));
 }
 song.g[G_CLOCK]=0;seq_start();tick(0);REQUIRE(clock_step()==0);seq_stop();tick(0);REQUIRE(seen("STOP"));
 /* Actual USB/TRS realtime packets, not a UI timer. */
 for(unsigned src=1;src<=2;src++) {
  song.g[G_CLOCK]=src;TSEL->p[P_SLEN]=16;TSEL->p[P_SDIV]=2;song.g[G_SWING]=0;
  events_block(CTL);midi_enqueue(0xfu|(0xfau<<8),src);events_block(CTL);ui_draw();REQUIRE(song.playing && clock_step()==0);
  for(unsigned n=0;n<30;n++)events_block(CTL);ui_draw();REQUIRE(clock_step()==0);
  unsigned mask=0;
  for(unsigned n=0;n<100;n++) {
   fm1_ms+=21;midi_enqueue(0xfu|(0xf8u<<8),src);events_block(CTL);ui_draw();
   REQUIRE((clock_visuals.beat&31u)==(TSEL->seq_idx&15u));mask|=1u<<clock_step();
  }
  REQUIRE(mask==65535u);
  midi_enqueue(0xfu|(0xfcu<<8),src);events_block(CTL);ui_draw();REQUIRE(!song.playing && !(clock_visuals.beat&32u));
  midi_enqueue(0xfu|(0xfau<<8),src);events_block(CTL);ui_draw();REQUIRE(clock_step()==0);
  midi_enqueue(0xfu|(0xfcu<<8),src);events_block(CTL);
 }
 song.g[G_CLOCK]=0;song.g[G_BPM]=240;song.g[G_SWING]=50;events_block(CTL);seq_start();
 unsigned mask=0;for(unsigned n=0;n<4000;n++){fm1_ms++;events_block(CTL);ui_draw();mask|=1u<<clock_step();REQUIRE((clock_visuals.beat&31u)==clock_step());}
 REQUIRE(mask==65535u);seq_stop();
 /* Physical controls still edit the original three globals. */
 song.g[G_BPM]=120;song.g[G_SWING]=40;song.g[G_CLOCK]=0;
 for(unsigned k=0;k<3;k++){int old=song.g[CLOCK_ID[k]];web_enc(EN_K1+k,1);ui_input();ui_draw();REQUIRE(song.g[CLOCK_ID[k]]!=old);}
 uint32_t prefs=ui_prefs;ui_prefs|=PREF_LARGE;ui.force=1;tick(0);REQUIRE(large_kind()==LK_LABEL);ui_prefs=prefs;
 TSEL->step[0].n=1;TSEL->step[0].time=ST_NOTE;TSEL->step[0].note[0]=60;ui.bank=0;ui.help_t=0;ui.force=1;tick(0);
 REQUIRE(web_fb[(Y_FOOT+1)*240+(unsigned)bar_x(0)+4]==swap16(HOME_ACCENT[song.sel&3u]));
 if(argc>1)picture(argv[1]);
 home_tap();ui_draw();REQUIRE(ui.home && !clock_visuals.valid);unsigned held=clock_visuals.position[0];tick(100);REQUIRE(clock_visuals.position[0]==held);
 REQUIRE(!bad_rects && !clipped_text);
 printf("CLOCK visuals: %u checks passed; cache %zu B; no new bitmap bytes\n",checks,sizeof clock_visuals);return 0;
}
