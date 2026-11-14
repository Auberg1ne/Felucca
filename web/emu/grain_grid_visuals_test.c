/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned checks,bad_rects,bad_ink,text_n,blits,pixels;
static char texts[128][32];
static void grid_watch_ink(int x0,int y0,int x1,int y1,const char*s,unsigned flags,unsigned w,unsigned h){
 if(text_n<128)snprintf(texts[text_n++],32,"%s",s);
 if(w==57 && h==50 && (x0<2 || y0<2 || x1>55 || y1>48 || (flags&3))){bad_ink++;fprintf(stderr,"ink %s: %d %d %d %d flags%u\n",s,x0,y0,x1,y1,flags);}
}
static void rect(unsigned x,unsigned y,unsigned w,unsigned h){blits++;pixels+=w*h;bad_rects+=x+w>240 || y+h>240 || w*h>240u*124u;}
#define GFX_HOOK_TEXT(x0,y0,x1,y1,s,flags) grid_watch_ink(x0,y0,x1,y1,s,flags,cv_w,cv_h)
#define GFX_HOOK_BLIT(x,y,r0) rect(x,y,cv_w,cv_h)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#include "felucca_web.c"
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"grid visuals %u: %s (bounds=%u ink=%u)\n",__LINE__,#x,bad_rects,bad_ink);return 1;}}while(0)
static int seen(const char*s){for(unsigned i=0;i<text_n;i++)if(!strcmp(texts[i],s))return 1;return 0;}
static void draw(void){text_n=0;ui.force=1;ui_draw();}
static void picture(const char*dir,unsigned page){char p[512];snprintf(p,sizeof p,"%s/grain-%u.ppm",dir,page);FILE*f=fopen(p,"wb");if(!f)return;
 fprintf(f,"P6\n240 240\n255\n");for(unsigned i=0;i<240*240;i++){unsigned v=swap16(web_fb[i]);unsigned char rgb[]={((v>>11)&31)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(rgb,3,1,f);}fclose(f);}
int main(int argc,char**argv){
 web_nor_erase();web_boot();while(fm1_ms<3500)web_ms();open_family(FAM_EDIT);
 static const char*labels[2][12]={{"SRC","POS","SIZE","DENS","PTCH","G-SPRD","RAND","TONE","STRETCH","--","--","--"},{"TYPE","DRIFT","WIDTH","--","CUTOFF","RESON","--","--","VCE","DTUNE","V-SPRD","--"}};
 static const char*personality[4][2]={{"DRIFT","WIDTH"},{"HOLD","MOTION"},{"RATE","GATE"},{"INTERVAL","HARMONICS"}};
 for(unsigned page=0;page<2;page++) {
  TSEL->p[P_GTYPE]=0;draw();for(unsigned i=0;i<12;i++)REQUIRE(seen(labels[page][i]));
  if(argc>1)picture(argv[1],page+1);
  for(unsigned row=0;row<3;row++){
   grain_edit_row=row;draw();REQUIRE(web_fb[(28+54*row)*240+8]==swap16(GRAIN_ACCENT[song.sel]));
   for(unsigned col=0;col<4;col++){
    unsigned id=grain_grid_param(page,row,col);if(id>=P_COUNT)continue;
    const param_desc_t*d=track_desc(TSEL,id);int old=TSEL->p[id];
    for(int value=d->min;value<=d->max;value++) {TSEL->p[id]=value;draw();char b[16];const char*u;param_format(track_desc(TSEL,id),value,b,&u);if(id==P_GB && TSEL->p[P_GTYPE]==3){strcat(b,u);}
    REQUIRE(seen(b));}
    TSEL->p[id]=old;
   }
  }
  if(page)for(unsigned type=0;type<4;type++){TSEL->p[P_GTYPE]=type;draw();REQUIRE(seen(personality[type][0]) && (type==3?(seen("HARMO") && seen("NICS")):seen(personality[type][1])));}
  open_family(FAM_EDIT);
 }
 REQUIRE(!bad_rects && !bad_ink);
 grain_edit_row=0;draw();unsigned n=web_draws;for(unsigned i=0;i<8;i++)ui_draw();REQUIRE(web_draws==n);
 blits=pixels=0;TSEL->p[P_E2]++;ui_draw();REQUIRE(blits==1 && pixels==57*50);
 go_home();ui_draw();REQUIRE(!grain_grid.valid);
 printf("GRAIN grid visuals: %u checks passed; full value sweep, dynamic labels and 240x240 bounds\n",checks);return 0;
}
