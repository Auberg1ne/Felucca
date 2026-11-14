/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned checks,bad_rects,bad_ink,overlaps,text_n,context;
static int row_y,row_h,label_right;
static char text_seen[128][32];
static void align_text(int y,int end,const char *tag){
 if(!strcmp(tag,"menu row name centred up/down")){context=1;row_y=y;row_h=end-y;}
 else if(!strcmp(tag,"menu row value centred up/down"))context=2;
}
static void settings_watch_ink(int x0,int y0,int x1,int y1,const char*s,int oy){
 if(text_n<128)snprintf(text_seen[text_n++],32,"%s",s);
 if(context){bad_ink+=x0<0 || x1>240 || y0-oy<row_y || y1-oy>row_y+row_h;
  if(context==1)label_right=x1;else overlaps+=x0<label_right+6;context=0;}
}
static void rect(unsigned x,unsigned y,unsigned w,unsigned h){bad_rects+=x+w>240 || y+h>240 || w*h>240u*124u;}
#define GFX_HOOK_ALIGN(x0,y0,x1,y1,flags,tag) align_text(y0,y1,tag)
#define GFX_HOOK_PEN(y,f) ((void)0)
#define GFX_HOOK_ITEM(x0,y0,x1,y1) ((void)0)
#define GFX_HOOK_ICON(x,y,size,id) ((void)0)
#define GFX_HOOK_TEXT(x0,y0,x1,y1,s,flags) settings_watch_ink(x0,y0,x1,y1,s,cv_oy)
#define GFX_HOOK_BLIT(x,y,r0) rect(x,y,cv_w,cv_h)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#include "felucca_web.c"
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"settings visuals %u: %s (bounds=%u ink=%u overlap=%u)\n",__LINE__,#x,bad_rects,bad_ink,overlaps);return 1;}}while(0)
static int seen(const char*s){for(unsigned i=0;i<text_n;i++)if(!strcmp(text_seen[i],s))return 1;return 0;}
static void draw(unsigned row){ui.menu=1;ui.menu_sel=row;text_n=context=0;ui.force=1;ui_draw();}
static void picture(const char *dir,unsigned tab){
 char path[512];snprintf(path,sizeof path,"%s/settings-%s.ppm",dir,MTAB_NAME[tab]);FILE*f=fopen(path,"wb");if(!f)return;
 fprintf(f,"P6\n240 240\n255\n");for(unsigned i=0;i<240*240;i++){unsigned v=swap16(web_fb[i]);unsigned char rgb[]={((v>>11)&31)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(rgb,3,1,f);}fclose(f);
}
int main(int argc,char **argv){
 web_nor_erase();web_boot();while(fm1_ms<3500)web_ms();
 static const unsigned colors[]={0x07ff,0xffe0,0xf81f,0x07e0,0xa81f};
 for(unsigned t=0;t<5;t++){
  unsigned row=menu_visible_row(t,0);draw(row);REQUIRE(SETTINGS_ACCENT[t]==colors[t]);
  REQUIRE(web_fb[86*240+8]==swap16(colors[t]));REQUIRE(web_fb[57*240+17+t*46]==swap16(colors[t]));
  REQUIRE(seen("COFFEE GRAINS") && seen("CLOSE") && seen(MTAB_NAME[t]));
  if(argc>1)picture(argv[1],t);
  for(unsigned i=0;i<menu_visible_count(t);i++) {
   row=menu_visible_row(t,i);draw(row);REQUIRE(seen(MI_NAME[row]));
   int selected_y=78+(int)i*((menu_visible_count(t)>6?18:20)+2);
   REQUIRE(web_fb[(selected_y+8)*240+8]==swap16(colors[t]));
   if(!menu_valued(row))continue;
   for(unsigned value=0;value<menu_n(row);value++) {
    menu_put(row,value);draw(row);REQUIRE(seen(MI_NAME[row]));
    char expected[24];snprintf(expected,sizeof expected,"%s%s",menu_vname(row,value),row==MI_TUNE?" ct":"");
    REQUIRE(seen(expected));
    if(menu_n(row)==2 && !MI_TEXT(row) && (str_eq(menu_vname(row,value),"ON") || str_eq(menu_vname(row,value),"OFF"))) {
      int y=78+(int)i*((menu_visible_count(t)>6?18:20)+2),h=menu_visible_count(t)>6?18:20,on=str_eq(menu_vname(row,value),"ON");
      REQUIRE(seen(on?"ON":"OFF"));REQUIRE(web_fb[(y+(h-12)/2+6)*240+(on?217:197)]==swap16(on?0:0xffff));
    }
   }
  }
 }
 menu_put(MI_HOLD,3);draw(MI_HOLD);REQUIRE(seen("0.6 s"));menu_put(MI_TUNE,100);draw(MI_TUNE);REQUIRE(seen("+50 ct"));
 menu_put(MI_SCROFF,4);draw(MI_SCROFF);REQUIRE(seen("60 MIN"));menu_put(MI_TCH4,16);draw(MI_TCH4);REQUIRE(seen("OMNI"));
 for(unsigned i=0;i<ML_N;i++)REQUIRE(ml_learn(70+i,0,P_ATK+i)==1);
 draw(MI_LCLEAR);REQUIRE(seen("16 CC"));
 draw(MI_SCROFF);menu_put(MI_SCROFF,0);text_n=context=0;ui_draw();REQUIRE(seen("NEVER"));
 unsigned draws=web_draws;for(unsigned n=0;n<8;n++)ui_draw();REQUIRE(web_draws==draws);
 REQUIRE(!bad_rects && !bad_ink && !overlaps);
 printf("settings visuals: %u checks passed; no row ink loss, overlap or bounds violations; bitmap bytes 0\n",checks);return 0;
}
