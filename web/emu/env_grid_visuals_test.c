/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned checks,bad_rects,bad_ink,text_n,blits,pixels,ink_luma,bad_contrast;
static char texts[128][32];
static void grid_watch_ink(int x0,int y0,int x1,int y1,const char*s,unsigned flags,unsigned w,unsigned h,const uint16_t *px){
 if(text_n<128)snprintf(texts[text_n++],32,"%s",s);
 if(w==57 && h==50){
  unsigned brightest=0;
  for(int y=y0<0?0:y0;y<y1 && y<(int)h;y++)for(int x=x0<0?0:x0;x<x1 && x<(int)w;x++){
   unsigned v=px[y*w+x],c=(v>>8)|((v&255)<<8);
   unsigned l=((c>>11)*255u/31u*54u+((c>>5)&63)*255u/63u*183u+(c&31)*255u/31u*19u)>>8;
   if(l>brightest)brightest=l;
  }
  /* The unchanged dim empty marker peaks at 94 on the light-palette AA curve. */
  if(ink_luma<100 || brightest<(!strcmp(s,"--")?90u:150u)){bad_contrast++;fprintf(stderr,"contrast %s: ink%u brightest%u\n",s,ink_luma,brightest);}
 }
 if(w==57 && h==50 && (x0<2 || y0<2 || x1>55 || y1>48 || (flags&3))){bad_ink++;fprintf(stderr,"ink %s: %d %d %d %d flags%u\n",s,x0,y0,x1,y1,flags);}
}
static void rect(unsigned x,unsigned y,unsigned w,unsigned h){blits++;pixels+=w*h;bad_rects+=x+w>240 || y+h>240 || w*h>240u*124u;}
#define GFX_HOOK_INK(fg,bg) (ink_luma=ux.mono?((fg)>>11)*255u/31u:ux_luma(fg))
#define GFX_HOOK_TEXT(x0,y0,x1,y1,s,flags) grid_watch_ink(x0,y0,x1,y1,s,flags,cv_w,cv_h,cv_px)
#define GFX_HOOK_BLIT(x,y,r0) rect(x,y,cv_w,cv_h)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#ifndef ENV_WEB_SOURCE
#define ENV_WEB_SOURCE "felucca_web.c"
#endif
#include ENV_WEB_SOURCE
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"ENV visuals %u: %s bounds%u ink%u\n",__LINE__,#x,bad_rects,bad_ink);return 1;}}while(0)
static int seen(const char*s){for(unsigned i=0;i<text_n;i++)if(!strcmp(texts[i],s))return 1;return 0;}
static void draw(void){text_n=0;ui.help_t=ui.msg_t=0;ui_draw();}
static int picture(const char*dir,const char*name){char path[1024];snprintf(path,sizeof path,"%s/%s.ppm",dir,name);FILE*f=fopen(path,"wb");if(!f)return 1;
 fprintf(f,"P6\n240 240\n255\n");for(unsigned i=0;i<240*240;i++){unsigned v=swap16(web_fb[i]);unsigned char rgb[]={((v>>11)&31)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(rgb,3,1,f);}return fclose(f)!=0;}
int main(int argc,char**argv){
 web_nor_erase();web_boot();while(fm1_ms<3500)web_ms();
 if(argc==3) {
  /* Exact non-ENV frames, including both GRAIN pages and HOME/MIXER/CLOCK. */
  FILE*f=fopen(argv[2],"wb");REQUIRE(f);
  for(unsigned ch=0;ch<4;ch++)for(unsigned i=0;i<NPAGES;i++) {
   song.sel=ch;if(PAGES[i].fam==FAM_ENV || !page_visible(i))continue;
   ui.home=0;ui.page=i;page_entered();ui.force=1;draw();fwrite(web_fb,sizeof web_fb,1,f);
  }
  for(unsigned ch=0;ch<4;ch++){song.sel=ch;go_home();ui.force=1;draw();fwrite(web_fb,sizeof web_fb,1,f);}
  fclose(f);open_family(FAM_ENV);ui.force=1;draw();REQUIRE(!picture(argv[1],"env-adsr"));
#ifdef ENV_BASE_CAPTURE
  open_family(FAM_ENV);ui.force=1;draw();REQUIRE(!picture(argv[1],"env-dest"));return 0;
#else
  grain_edit_row=1;ui.force=1;draw();REQUIRE(!picture(argv[1],"env-row2"));
#endif
 }
#ifndef ENV_BASE_CAPTURE
 open_family(FAM_ENV);
 const char*labels[]={"ATK","DEC","SUS","REL","FLT","PIT","SHP","ESYNC"};
 for(unsigned ch=0;ch<4;ch++)for(unsigned row=0;row<2;row++) {
  song.sel=ch;grain_edit_row=row;ui.force=1;draw();for(unsigned k=0;k<8;k++)REQUIRE(seen(labels[k]));
  REQUIRE(web_fb[(28+54*row)*240+30]==swap16(GRAIN_ACCENT[ch]));
  for(unsigned c=0;c<4;c++) {
   unsigned id=env_grid_param(row,c);const param_desc_t*d=track_desc(TSEL,id);int old=TSEL->p[id];
   for(int v=d->min;v<=d->max;v++){TSEL->p[id]=v;draw();char b[16];const char*u;param_format(d,v,b,&u);REQUIRE(seen(b) || v==old);}
   TSEL->p[id]=old;draw();
  }
 }
 /* Incremental graph and cells exactly equal a fresh complete redraw. */
 static uint16_t before[240*240];
 for(unsigned n=0;n<128;n++){
  TSEL->p[P_ESYNC]=n%2;
  TSEL->p[P_ATK]=n;TSEL->p[P_DEC]=127-n;TSEL->p[P_SUS]=(n*5)%128;TSEL->p[P_REL]=(n*7)%128;
  draw();memcpy(before,web_fb,sizeof before);ui.force=1;draw();REQUIRE(!memcmp(before,web_fb,sizeof before));
 }
 REQUIRE(!bad_rects && !bad_ink && !bad_contrast);
 unsigned n=web_draws;for(unsigned k=0;k<8;k++)draw();REQUIRE(web_draws==n);
#endif
 printf("ENV visuals: %u checks passed; native 240x240, all labels, full value ranges, incremental redraw and bounds\n",checks);return 0;
}
