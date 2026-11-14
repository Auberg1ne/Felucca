/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned checks,bad_rects,names_n,row_pending;
static char names[32][32];
static void row_align(const char *what){if(!strcmp(what,"menu row name centred up/down"))row_pending=1;}
static void row_text(const char *text){if(row_pending){if(names_n<32)snprintf(names[names_n++],32,"%s",text);row_pending=0;}}
static void bounds(unsigned x,unsigned y,unsigned w,unsigned h){bad_rects+=x+w>240 || y+h>240 || w*h>240u*124u;}
#define GFX_HOOK_ALIGN(x0,y0,x1,y1,flags,what) row_align(what)
#define GFX_HOOK_PEN(y,f) ((void)0)
#define GFX_HOOK_ITEM(x0,y0,x1,y1) ((void)0)
#define GFX_HOOK_ICON(x,y,size,id) ((void)0)
#define GFX_HOOK_TEXT(x0,y0,x1,y1,s,flags) row_text(s)
#define GFX_HOOK_BLIT(x,y,r0) bounds(x,y,cv_w,cv_h)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#include "felucca_web.c"
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"settings bounds %u: %s\n",__LINE__,#x);return 1;}}while(0)
static int seen(const char *text){for(unsigned i=0;i<names_n;i++)if(!strcmp(names[i],text))return 1;return 0;}
int main(void)
{
 static const unsigned hidden[]={MI_COLOR,MI_STYLE,MI_LARGE,MI_SCOPE,MI_HOME,MI_CHADD,MI_SELECT,MI_MFOLLOW,MI_SCRWAKE};
 web_nor_erase();web_boot();while(fm1_ms<3500)web_ms();ui.menu=1;
 for(unsigned p=0;p<NPALETTES;p++)for(unsigned style=0;style<2;style++)for(unsigned large=0;large<2;large++) {
  palette_choose(p);ui_style=style;ui_prefs=large?PREF_LARGE:0;
  for(unsigned t=0;t<MTAB_COUNT;t++)for(unsigned i=0;i<menu_visible_count(t);i++) {
   ui.menu_sel=menu_visible_row(t,i);names_n=row_pending=0;ui.force=1;ui_draw();REQUIRE(seen(MI_NAME[ui.menu_sel]));
   /* HOME CLOSE is a preserved header key hint, not the hidden HOME setting. */
   for(unsigned h=0;h<NELEM(hidden);h++)REQUIRE(!seen(MI_NAME[hidden[h]]));
  }
 }
 REQUIRE(!bad_rects);printf("settings menu: %u checks passed across all palettes/styles/LARGE, no hidden rows or 240x240 bounds violations\n",checks);return 0;
}
