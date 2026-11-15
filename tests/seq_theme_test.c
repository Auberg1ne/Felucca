/* SPDX-License-Identifier: GPL-3.0-only */
#define COFFEE_HOME_VISUALS 1
#define UI_TEST_NO_MAIN 1
#ifndef SEQ_UI_SOURCE
#define SEQ_UI_SOURCE "ui_test.c"
#endif
#include SEQ_UI_SOURCE
static unsigned checks,states;static FILE*screens;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"SEQ theme %u: %s\n",__LINE__,#x);return 1;}}while(0)
static void state(FILE*f){static project_t p;memset(&p,0,sizeof p);project_capture(&p);
 unsigned s[]={ui.home,ui.page,ui.layer,ui.lock,ui.ly,ui.cursor,ui.bank,song.sel,song.playing,song.rec,transport_req,ui.confirm,ui.act,
 ui.ev_row,ui.ev_id,ui.ev_step,ui.ev_keep,ui.ev_vel,eu.on,eu.hits,eu.steps,eu.rot,proll.lo};
 if(f){fwrite(&p,sizeof p,1,f);fwrite(s,sizeof s,1,f);for(unsigned t=0;t<4;t++)fwrite(trk[t].p,sizeof trk[t].p,1,f);}
 if(screens){uint32_t target=!ui.confirm&&!ui.menu&&!name_on()&&(ui.layer==LAYER_SEQ||(!ui.home&&!ui.layer&&cur_page()->fam==FAM_SEQ));fwrite(&target,4,1,screens);fwrite(host_screen,sizeof host_screen,1,screens);}states++;
}
static void pattern(void){track_defaults_steps(TSEL);TSEL->p[P_SLEN]=16;for(unsigned i=0;i<16;i++){
 step_t*s=&TSEL->step[i];s->time=i%5==1?ST_REST:i%5==2?ST_TIE:ST_NOTE;if(s->time==ST_NOTE){s->n=1;s->note[0]=48+i*3;s->vel=90;}
 s->flags=(i&1?SF_ACCENT:0)|(i%3==0?SF_SLIDE:0);ev_sput(s,EVK_CHANCE,i%2?30:100);ev_sput(s,EVK_RATCH,1+i%4);ev_sput(s,EVK_NUDGE,(int)i%16-8);}
 motion_clear(TSEL);motion_set_event(TSEL,3,P_REV,70);motion_set_lock(TSEL,5,P_E1,80);
}
int main(int argc,char**argv){FILE*f=argc>1?fopen(argv[1],"wb"):NULL;if(argc>1&&!f)return 2;if(f){char p[1024];snprintf(p,sizeof p,"%s.frames",argv[1]);screens=fopen(p,"wb");if(!screens)return 2;}
 for(unsigned t=0;t<4;t++){
  ui_power_on();track_select(t);pattern();press(B_SEQ);REQUIRE(!detail_on()&&cur_page()->graph==GR_ROLL&&!ui.layer);state(f);
  for(unsigned i=0;i<16;i++){cursor_set(i);frame();turn(EN_K2,i&1?1:-1);turn(EN_K3,i&1?1:-1);turn(EN_K4,1);state(f);turn(EN_K1,1);state(f);}
  frames(400);press(B_SEQ);REQUIRE(detail_on());state(f);
  for(unsigned i=0;i<16;i++){cursor_set(i);for(unsigned c=0;c<4;c++){turn(EN_K1+c,i&1?127:-127);state(f);}}
  press(B_EDIT);state(f);frames(400);press(B_SEQ);REQUIRE(cur_page()->graph==GR_EVENTS);state(f);
  for(unsigned row=0;row<6;row++){turn(EN_K1,1);turn(EN_K2,1);turn(EN_K3,1);turn(EN_K4,row&1?-1:1);state(f);}
  /* CLEAR confirmation and cancel remain the existing Automation action. */
  ui.ev_row=0;ui.force=1;frame();press(B_OCTUP);state(f);if(ui.confirm){REQUIRE(ui.confirm==CF_CLEAR_MOTION);press(B_OCTDN);state(f);}
  uint16_t rows[EV_ROWS];ui.ev_row=(uint16_t)(ev_list(rows)-1);ui.ev_id=P_E1;ui.ev_step=2;frame();press(B_OCTUP);state(f);press(B_EDIT);state(f);
  press(B_OCTDN);state(f);go_home();frames(400);
  /* All GRAIN-available tools retain their original key coordinates and undo. */
  const unsigned tools[]={TL_REV,TL_LEFT,TL_RIGHT,TL_RANDOM,TL_COOK,TL_DOUBLE,TL_HALF,TL_CLEAR};
  for(unsigned i=0;i<NELEM(tools);i++){
   pattern();btn_down(B_SEQ);frame();key_down(white(tools[i]));frame();REQUIRE(ui.layer==LAYER_SEQ);state(f);
   key_up(white(tools[i]));frame();oct_back();state(f);btn_up(B_SEQ);frame();hold(B_SAVE);state(f);frames(400);
  }
  pattern();lay_combo(B_SEQ,white(TL_EUCLID));REQUIRE(eu.on);state(f);for(unsigned c=0;c<3;c++){turn(EN_K1+c,1);state(f);}
  key_up(white(TL_EUCLID));frame();btn_up(B_SEQ);frame();REQUIRE(!eu.on);state(f);
  go_home();frames(400);press(B_SEQ);press(B_SEQ);REQUIRE(ui.lock==LAYER_SEQ);state(f);press(B_SEQ);REQUIRE(!ui.lock&&!ui.layer);state(f);
  go_home();frames(400);btn_down(B_SEQ);frames(800);REQUIRE(ui.layer==LAYER_SEQ);state(f);btn_up(B_SEQ);frame();state(f);
  pattern();go_title("STEP");song.rec=1u<<t;cursor_set(0);tap_key(white(0));state(f);song.rec=0;
  seq_start();seq_tick(TSEL,CTL);frame();state(f);events_block(CTL*3);frame();state(f);seq_stop();frame();state(f);
  /* Entered-note hold + knob creates a real lock from the remembered ENV page. */
  frames(400);REQUIRE(!layer_knobs_quiet());go_title("ENV");go_title("STEP");song.rec=1u<<t;cursor_set(0);key_down(white(0));frame();REQUIRE(lock_held());
  unsigned ls=ui.cursor,id=lock_id(0);turn(EN_K1,5);int16_t lv;REQUIRE(motion_lock_get(TSEL,ls,id,&lv));state(f);
  press(B_EDIT);state(f);key_up(white(0));frame();song.rec=0;
  seq_start();seq_tick(TSEL,CTL);btn_down(B_SEQ);frame();key_down(white(TL_REV));frame();REQUIRE(ui.layer==LAYER_SEQ);state(f);
  key_up(white(TL_REV));btn_up(B_SEQ);frame();seq_stop();state(f);
  REQUIRE(!project_save(t));static project_t p,q;static project_store_t stored;project_capture(&p);REQUIRE(proj_pack(&stored,&p)&&proj_import(&q,&stored,sizeof stored));REQUIRE(!project_restore_runtime(&q));state(f);
 }
 if(screens)REQUIRE(!fclose(screens));if(f)REQUIRE(!fclose(f));printf("SEQ interactions: %u assertions passed; %u state checkpoints\n",checks,states);return 0;
}
