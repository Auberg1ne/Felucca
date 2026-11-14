/* SPDX-License-Identifier: GPL-3.0-only */
#define COFFEE_HOME_VISUALS 1
#define UI_TEST_NO_MAIN 1
#ifndef GRID_UI_SOURCE
#define GRID_UI_SOURCE "ui_test.c"
#endif
#include GRID_UI_SOURCE
int main(int argc,char**argv){
 if(argc!=2)return 2;ui_power_on();kb_boot_hold=0;set_engine_of(TSEL,ENGI_SAMPLE);TSEL->engine=ENGI_SAMPLE;apply_preset_to(TSEL,0);
 track_t*t=TSEL;memset(t->step,0,sizeof t->step);t->p[P_SLEN]=16;
 const unsigned notes[]={0,3,7,12};for(unsigned i=0;i<NELEM(notes);i++){t->step[notes[i]].n=1;t->step[notes[i]].note[0]=60+i;}
 t->step[3].flags=SF_ACCENT;t->step[1].time=ST_TIE;t->step[4].time=ST_REST;t->step[9].hit=1;
 song.rec=0;seq_start();seq_tick(TSEL,CTL);events_block(CTL);
 for(unsigned row=0;row<2;row++){
#ifdef SAMPLE_GRID_BASE
  go_title(row?"VOICE":"EDIT 1");
#else
  go_title("EDIT 1");if(row)turn(EN_SELECT,1);
#endif
  screen_save(argv[1],row?"sample-row-2":"sample-row-1");
 }
 printf("SAMPLE preview: 240x240, actual mixed steps, transport index %u\n",t->seq_idx);return 0;
}
