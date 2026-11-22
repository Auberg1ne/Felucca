/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"effect D %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char**argv){
 ui_power_on();press(B_EDIT);frames(400);press(B_EDIT);
 const char*names[]={"SPREAD","DRIFT","CHAOS","SPARKLE"};
 CHECK(grain_grid_page(cur_page())==2&&grain_edit_row==0);
 CHECK(grain_grid_param(1,0,0)==103&&grain_grid_param(1,0,1)==104&&grain_grid_param(1,0,2)==105&&grain_grid_param(1,0,3)==118);
 for(unsigned track=0;track<4;track++){
  track_select(track);
  for(unsigned type=0;type<4;type++){
   TSEL->p[P_GTYPE]=type;CHECK(!strcmp(grain_effect_label(TSEL),names[type]));CHECK(text_w(&AF_S,names[type])<=53);
   for(unsigned value=0;value<=100;value++){
    TSEL->p[P_GRC]=value?value-1:0;int16_t before[P_COUNT];memcpy(before,TSEL->p,sizeof before);
    ui.ml.on=1;ui.ml.pick=0;turn(EN_K4,value?1:-1);ui.ml.on=0;
    CHECK(TSEL->p[P_GRC]==(int)value&&!ui.ml.pick&&!motion.count);
    for(unsigned id=0;id<P_COUNT;id++)if(id!=P_GRC)CHECK(TSEL->p[id]==before[id]);
   }
   turn(EN_K4,5);CHECK(TSEL->p[P_GRC]==100);CHECK(lock_id(3)==255&&ml_learn(70,track,118)==2);
   /* Saved values and external/editor writes share the same canonical cell. */
   static project_t p,q;static project_store_t stored;TSEL->p[P_GRC]=55;project_capture(&p);
   CHECK(proj_pack(&stored,&p)&&proj_import(&q,&stored,sizeof stored));TSEL->p[P_GRC]=0;
   CHECK(!project_restore_runtime(&q)&&TSEL->p[P_GRC]==55);
   ui.force=1;draw_grain_grid();ui.force=0;static uint16_t old[240*240];memcpy(old,host_screen,sizeof old);
   TSEL->p[P_GRC]=25;draw_grain_grid();CHECK(grain_grid.sig[3]==((25u+64u)|(type<<9)|(118u<<12)));
   for(unsigned y=0;y<240;y++)for(unsigned x=0;x<240;x++)if(x<180||x>=237||y<28||y>=78)CHECK(host_screen[y*240+x]==old[y*240+x]);
   if(argc==2&&track==0){char path[512];snprintf(path,sizeof path,"%s/effect-D-%u.ppm",argv[1],type);FILE*f=fopen(path,"wb");CHECK(f);fprintf(f,"P6\n240 240\n255\n");for(unsigned i=0;i<240*240;i++){unsigned v=swap16(host_screen[i]);fputc(((v>>11)&31)*255/31,f);fputc(((v>>5)&63)*255/63,f);fputc((v&31)*255/31,f);}CHECK(!fclose(f));}
  }
 }
 CHECK(!strcmp(param_desc_of(ENGI_GRAIN,118)->label,"C"));
 printf("Effect D UI: %u checks passed; A/B/C preserved, K4 all values/types/tracks, saved values, dirty pixels and quarantine\n",checks);return 0;
}
