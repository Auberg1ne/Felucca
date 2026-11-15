/* SPDX-License-Identifier: GPL-3.0-only */
#define COFFEE_HOME_VISUALS 1
#define UI_TEST_NO_MAIN 1
#ifndef GLO_UI_SOURCE
#define GLO_UI_SOURCE "ui_test.c"
#endif
#include GLO_UI_SOURCE
static unsigned checks,states;
static FILE *screens;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"GLO theme %u: %s\n",__LINE__,#x);return 1;}}while(0)
static void state(FILE*f){
 static project_t p;memset(&p,0,sizeof p);project_capture(&p);
 unsigned s[]={ui.home,ui.page,ui.layer,ui.lock,ui.ly,ui.song_row,ui.song_trk,song.sel,song.playing,song.octave,
 perf_solo,seq_fill,transport_req,ui.ml.on,ui.ml.pick,ui.ml.id,chain.running,chain.row,chain.remaining,chain_config.count};
 if(f){fwrite(&p,sizeof p,1,f);fwrite(s,sizeof s,1,f);fwrite(&chain_config,sizeof chain_config,1,f);}
 if(screens){uint32_t target=ui.layer==LAYER_GLO || (!ui.home && cur_page()->graph==GR_SONG);fwrite(&target,sizeof target,1,screens);fwrite(host_screen,sizeof host_screen,1,screens);}
 states++;
}
int main(int argc,char**argv){FILE*f=argc>1?fopen(argv[1],"wb"):NULL;if(argc>1&&!f)return 2;
 if(f){char path[1024];snprintf(path,sizeof path,"%s.frames",argv[1]);screens=fopen(path,"wb");if(!screens)return 2;}
 for(unsigned track=0;track<4;track++){
  ui_power_on();track_select(track);press(B_GLO);REQUIRE(cur_page()->graph==GR_SONG&&!ui.layer);state(f);
  /* Projects A-D, each with real four-track patterns and lengths. */
  for(unsigned slot=0;slot<4;slot++){
   for(unsigned t=0;t<4;t++){track_defaults_steps(&trk[t]);trk[t].p[P_SLEN]=8+slot*4+t;trk[t].step[slot].n=1;trk[t].step[slot].note[0]=60+slot+t;trk[t].step[slot].vel=100;}
   REQUIRE(!project_save(slot));
  }
  for(unsigned section=0;section<4;section++){
   turn(EN_K1,1);turn(EN_K2,(int)section);turn(EN_K3,1);turn(EN_K4,1);state(f);
  }
  /* Existing SONG editor routes are compared to the exact base, not reinterpreted. */
  for(unsigned c=0;c<4;c++){turn(EN_K1,c?1:-1);turn(EN_K2,1);turn(EN_K3,c%2?1:-1);turn(EN_K4,2);state(f);}
  press(B_SAVE);state(f);hold(B_SAVE);state(f);go_home();frames(400);
  /* All four mutes, SOLO held through GLO release, ALL and level UNDO. */
  for(unsigned t=0;t<4;t++){
   lay_combo(B_GLO,black(t));REQUIRE(ui.layer==LAYER_GLO&&trk[t].p[P_MUTE]);state(f);key_up(black(t));btn_up(B_GLO);frame();state(f);
  }
  lay_combo(B_GLO,white(4));for(unsigned t=0;t<4;t++)REQUIRE(!trk[t].p[P_MUTE]);state(f);key_up(white(4));btn_up(B_GLO);frame();
  for(unsigned t=0;t<4;t++){
   lay_combo(B_GLO,white(t));perf_begin(CTL);REQUIRE(perf_solo==(1u<<t));state(f);btn_up(B_GLO);frame();REQUIRE(ui.layer==LAYER_GLO);state(f);
   key_up(white(t));frame();perf_begin(CTL);REQUIRE(!perf_solo&&!ui.layer);state(f);
  }
  int16_t level[4];for(unsigned t=0;t<4;t++)level[t]=trk[t].p[P_LEVEL];
  btn_down(B_GLO);frame();for(unsigned t=0;t<4;t++)turn(EN_K1+t,-7);state(f);oct_back();for(unsigned t=0;t<4;t++)REQUIRE(trk[t].p[P_LEVEL]==level[t]);state(f);btn_up(B_GLO);frame();
  /* LEARN toggles; FILL is held; TAP uses real monotonic event timing. */
  lay_combo(B_GLO,white(5));REQUIRE(ui.ml.on);state(f);key_up(white(5));frame();key_down(white(5));frame();REQUIRE(!ui.ml.on);key_up(white(5));frame();
  key_down(white(6));frame();REQUIRE(seq_fill);state(f);key_up(white(6));frame();REQUIRE(!seq_fill);state(f);btn_up(B_GLO);frame();
  song.g[G_CLOCK]=0;song.g[G_BPM]=100;btn_down(B_GLO);frame();for(unsigned t=0;t<3;t++){key_down(white(7));frame();key_up(white(7));frames(480);}REQUIRE(song.g[G_BPM]==120);state(f);btn_up(B_GLO);frame();
  song.g[G_CLOCK]=1;lay_combo(B_GLO,white(7));REQUIRE(song.g[G_BPM]==120);state(f);key_up(white(7));btn_up(B_GLO);frame();song.g[G_CLOCK]=0;
  song.playing=1;transport_req=0;btn_down(B_GLO);frame();press(B_PLAY);REQUIRE(transport_req==3);state(f);events_block(CTL);btn_up(B_GLO);frame();stop_transport();
  frames(400);go_home();press(B_GLO);press(B_GLO);REQUIRE(ui.lock==LAYER_GLO&&ui.layer==LAYER_GLO);state(f);
  press(B_PLAY);REQUIRE(transport_req==1);state(f);transport_req=0;press(B_GLO);state(f);REQUIRE(!ui.layer&&!ui.lock);
 }
 if(screens)REQUIRE(!fclose(screens));if(f)REQUIRE(!fclose(f));printf("GLO interactions: %u assertions passed; %u serialized state checkpoints\n",checks,states);return 0;
}
