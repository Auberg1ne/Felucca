/* SPDX-License-Identifier: GPL-3.0-only */
/* Reuse the grid's existing glyph/pixel/bounds probe, not its GRAIN scenarios. */
#define GRAIN_GRID_VISUALS_NO_MAIN 1
#include "grain_grid_visuals_test.c"
static int blank(unsigned x,unsigned y,unsigned w,unsigned h){for(unsigned r=y;r<y+h;r++)for(unsigned c=x;c<x+w;c++)if(web_fb[r*240+c])return 0;return 1;}
int main(int argc,char**argv){
 web_nor_erase();web_boot();while(fm1_ms<3500)web_ms();
 for(unsigned ch=0;ch<4;ch++){set_engine_of(&trk[ch],ENGI_SAMPLE);trk[ch].engine=ENGI_SAMPLE;}
 open_family(FAM_EDIT);REQUIRE(sample_grid_page(cur_page()) && !edit_grid_row);
 const char*labels[]={"BITS","LOOP","CUT","VCE","DTUNE","SPRD"};
 const char*hidden[]={"SET","TUNE","DRV","GLD","GLMOD","PRIO","ALLOC","PAN","MUTE","G-SPRD","V-SPRD"};
 for(unsigned row=0;row<2;row++){
  edit_grid_row=row;draw();for(unsigned i=0;i<NELEM(labels);i++)REQUIRE(seen(labels[i]));
  for(unsigned i=0;i<NELEM(hidden);i++)REQUIRE(!seen(hidden[i]));
  REQUIRE(blank(180,28,60,104) && blank(0,136,240,50));
  for(unsigned col=0;col<3;col++){
   unsigned id=edit_grid_param(3,row,col);const param_desc_t*d=track_desc(TSEL,id);int old=TSEL->p[id];
   for(int v=d->min;v<=d->max;v++){TSEL->p[id]=v;draw();char b[16];const char*u;param_format(d,v,b,&u);REQUIRE(seen(b));}TSEL->p[id]=old;
  }
 }
 for(unsigned p=0;p<NPALETTES;p++)for(unsigned style=0;style<2;style++)for(unsigned ch=0;ch<4;ch++)for(unsigned row=0;row<2;row++){
  settings.palette=p;ui_style=style;palette_set(p);track_select(ch);edit_grid_row=row;draw();
  REQUIRE(web_fb[(28+54*row)*240+30]==swap16(GRAIN_ACCENT[ch]));
  REQUIRE(blank(180,28,60,104) && blank(0,136,240,50));
 }
 REQUIRE(!bad_rects && !bad_ink && !bad_contrast);
 /* The exact HOME helper must produce the same strip for the selected SAMPLE track. */
 static uint16_t strip[240*16];
 for(unsigned ch=0;ch<4;ch++){track_t*t=&trk[ch];memset(t->step,0,sizeof t->step);t->p[P_SLEN]=16;t->step[ch].n=1;t->step[ch].note[0]=60;
  t->step[4].hit=1;t->step[5].time=ST_TIE;t->step[6].time=ST_REST;t->step[6].n=1;t->step[7].n=1;t->step[7].flags=SF_ACCENT;}
 for(unsigned ch=0;ch<4;ch++)for(unsigned row=0;row<2;row++)for(unsigned playing=0;playing<2;playing++)for(unsigned ph=0;ph<16;ph++){
  track_select(ch);song.playing=playing;TSEL->seq_idx=ph;go_home();ui.help_t=0;ui.ml.on=0;ui.force=1;draw_foot();memcpy(strip,web_fb+Y_FOOT*240,sizeof strip);
  open_family(FAM_EDIT);edit_grid_row=row;ui.help_t=0;ui.force=1;ui_draw();REQUIRE(!memcmp(strip,web_fb+Y_FOOT*240,sizeof strip));
 }
 song.playing=0;draw();unsigned n=web_draws;for(unsigned i=0;i<8;i++)ui_draw();REQUIRE(web_draws==n);
 blits=pixels=0;TSEL->p[P_E2]++;ui_draw();REQUIRE(blits==1 && pixels==57*50);
 REQUIRE(!bad_rects && !bad_ink && !bad_contrast);
 printf("SAMPLE visuals: %u checks passed; full ranges, palettes/styles, blank areas, shared footer and bounds\n",checks);return 0;
}
