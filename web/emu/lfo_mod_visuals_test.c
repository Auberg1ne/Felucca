/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned checks,bad_rects,bad_ink,text_n,blits,pixels,ink_luma,bad_contrast;
static char texts[128][32];
static unsigned watch_target;
static void grid_watch_ink(int x0,int y0,int x1,int y1,const char*s,unsigned flags,unsigned w,unsigned h,const uint16_t *px){
 if(text_n<128)snprintf(texts[text_n++],32,"%s",s);
 if(watch_target && ((w==57 && (h==44 || h==50)) || (w==234 && h==26))){
  unsigned brightest=0;
  for(int y=y0<0?0:y0;y<y1 && y<(int)h;y++)for(int x=x0<0?0:x0;x<x1 && x<(int)w;x++){
   unsigned v=px[y*w+x],c=(v>>8)|((v&255)<<8);
   unsigned l=((c>>11)*255u/31u*54u+((c>>5)&63)*255u/63u*183u+(c&31)*255u/31u*19u)>>8;
   if(l>brightest)brightest=l;
  }
  /* The unchanged dim empty marker peaks at 94 on the light-palette AA curve. */
  if(ink_luma<100 || brightest<(!strcmp(s,"--")?90u:150u)){bad_contrast++;fprintf(stderr,"contrast %s: ink%u brightest%u\n",s,ink_luma,brightest);}
 }
 if(watch_target && w==234 && h==26 && (x0<2 || y0<2 || x1>232 || y1>24 || (flags&3))){bad_ink++;fprintf(stderr,"matrix ink %s: %d %d %d %d flags%u\n",s,x0,y0,x1,y1,flags);}
 if(watch_target && w==57 && (h==44 || h==50) && (x0<2 || y0<2 || x1>55 || y1>(int)h-2 || (flags&3))){bad_ink++;fprintf(stderr,"ink %s: %d %d %d %d flags%u\n",s,x0,y0,x1,y1,flags);}
}
static void rect(unsigned x,unsigned y,unsigned w,unsigned h){blits++;pixels+=w*h;bad_rects+=x+w>240 || y+h>240 || w*h>240u*124u;}
#define GFX_HOOK_INK(fg,bg) (ink_luma=ux.mono?((fg)>>11)*255u/31u:ux_luma(fg))
#define GFX_HOOK_TEXT(x0,y0,x1,y1,s,flags) grid_watch_ink(x0,y0,x1,y1,s,flags,cv_w,cv_h,cv_px)
#define GFX_HOOK_BLIT(x,y,r0) rect(x,y,cv_w,cv_h)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#ifndef LFO_MOD_WEB_SOURCE
#define LFO_MOD_WEB_SOURCE "felucca_web.c"
#endif
#include LFO_MOD_WEB_SOURCE
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"LFO MOD visuals %u: %s rect%u ink%u contrast%u\n",__LINE__,#x,bad_rects,bad_ink,bad_contrast);return 1;}}while(0)
static int seen(const char*s){for(unsigned i=0;i<text_n;i++)if(!strcmp(texts[i],s))return 1;return 0;}
static void draw(void){text_n=0;ui.help_t=ui.msg_t=0;ui_draw();}
static int picture(const char*dir,const char*name){char p[1024];snprintf(p,sizeof p,"%s/%s.ppm",dir,name);FILE*f=fopen(p,"wb");if(!f)return 1;
 fprintf(f,"P6\n240 240\n255\n");for(unsigned i=0;i<240*240;i++){unsigned c=swap16(web_fb[i]);unsigned char rgb[]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(rgb,3,1,f);}return fclose(f)!=0;}
int main(int argc,char**argv){
 web_nor_erase();web_boot();while(fm1_ms<3500)web_ms();REQUIRE(argc==3);FILE*f=fopen(argv[2],"wb");REQUIRE(f);unsigned shots=0;
 for(unsigned ch=0;ch<4;ch++)for(unsigned i=0;i<NPAGES;i++){
  song.sel=ch;if(PAGES[i].fam==FAM_LFO || PAGES[i].fam==FAM_SCL || !page_visible(i))continue;
  ui.page=i;ui.home=0;page_entered();ui.force=1;draw();fwrite(web_fb,sizeof web_fb,1,f);shots++;
 }
 for(unsigned ch=0;ch<4;ch++){song.sel=ch;go_home();ui.force=1;draw();fwrite(web_fb,sizeof web_fb,1,f);shots++;}
 REQUIRE(!fclose(f));song.sel=0;
#ifdef LFO_MOD_BASE_CAPTURE
 for(unsigned i=0;i<NPAGES;i++)if(PAGES[i].fam==FAM_LFO || PAGES[i].fam==FAM_SCL){
  ui.home=0;ui.page=i;page_entered();ui.force=1;draw();char name[32];snprintf(name,sizeof name,"old-%s",PAGES[i].title);REQUIRE(!picture(argv[1],name));
 }
#else
 watch_target=1;open_family(FAM_LFO);
 const char*labels[]={"RATE","WAVE","PHS","FADE","SYNC","TRIG","POL","--","PIT","FLT","SHP","AMP"};
 for(unsigned row=0;row<3;row++){
  grain_edit_row=row;ui.force=1;draw();for(unsigned k=0;k<12;k++)REQUIRE(seen(labels[k]));
  char name[32];snprintf(name,sizeof name,"lfo-row%u",row+1);REQUIRE(!picture(argv[1],name));
 }
 for(unsigned ch=0;ch<4;ch++)for(unsigned row=0;row<3;row++){
  song.sel=ch;grain_edit_row=row;ui.force=1;draw();REQUIRE(web_fb[(28+47*row)*240+30]==swap16(GRAIN_ACCENT[ch]));
  for(unsigned c=0;c<4;c++){
   unsigned id=lfo_grid_param(row,c);if(id>=P_COUNT)continue;const param_desc_t*d=track_desc(TSEL,id);int old=TSEL->p[id];
   for(int v=d->min;v<=d->max;v++){TSEL->p[id]=v;draw();char b[16];const char*u;param_format(d,v,b,&u);strcat(b,u);REQUIRE(seen(b)||v==old);}
   TSEL->p[id]=old;draw();
  }
 }
 static uint16_t before[240*240],wave[240*32];
 for(unsigned shape=0;shape<5;shape++)for(unsigned phase=0;phase<3;phase++)for(unsigned pol=0;pol<2;pol++){
  TSEL->p[P_LWAVE]=shape;TSEL->p[P_LPHASE]=phase*43;TSEL->p[P_LPOL]=pol;draw();memcpy(before,web_fb,sizeof before);ui.force=1;draw();
  if(memcmp(before,web_fb,sizeof before))for(unsigned z=0;z<240*240;z++)if(before[z]!=web_fb[z]){fprintf(stderr,"redraw x%u y%u old%x new%x page%u row%u\n",z%240,z/240,before[z],web_fb[z],ui.page,grain_edit_row);break;}
  REQUIRE(!memcmp(before,web_fb,sizeof before));
 }
 TSEL->p[P_LWAVE]=0;TSEL->p[P_LPOL]=0;TSEL->p[P_LPHASE]=0;draw();memcpy(wave,web_fb+169*240,sizeof wave);
 TSEL->p[P_LPHASE]=64;draw();REQUIRE(memcmp(wave,web_fb+169*240,sizeof wave));
 memcpy(wave,web_fb+169*240,sizeof wave);TSEL->p[P_LPOL]=1;draw();REQUIRE(memcmp(wave,web_fb+169*240,sizeof wave));
 unsigned last=web_draws;for(unsigned i=0;i<8;i++)draw();REQUIRE(web_draws==last);
 open_family(FAM_SCL);for(unsigned slot=0;slot<4;slot++){
  mod_ui_slot=slot;for(unsigned k=0;k<4;k++){TSEL->p[P_M1SRC+3*k]=k+1;TSEL->p[P_M1DST+3*k]=k+1;TSEL->p[P_M1AMT+3*k]=15*(k+1);}
  ui.force=1;draw();char name[32];snprintf(name,sizeof name,"matrix-slot%u",slot+1);REQUIRE(!picture(argv[1],name));
 }
 for(unsigned ch=0;ch<4;ch++)for(unsigned slot=0;slot<4;slot++){
  song.sel=ch;mod_ui_slot=slot;ui.force=1;draw();REQUIRE(web_fb[(82+29*slot)*240+30]==swap16(GRAIN_ACCENT[ch]));
  for(unsigned k=0;k<3;k++){
   unsigned id=P_M1SRC+3*slot+k;const param_desc_t*d=track_desc(TSEL,id);int old=TSEL->p[id];
   for(int v=d->min;v<=d->max;v++){TSEL->p[id]=v;draw();memcpy(before,web_fb,sizeof before);ui.force=1;draw();
  if(memcmp(before,web_fb,sizeof before))for(unsigned z=0;z<240*240;z++)if(before[z]!=web_fb[z]){fprintf(stderr,"redraw x%u y%u old%x new%x page%u row%u\n",z%240,z/240,before[z],web_fb[z],ui.page,grain_edit_row);break;}
  REQUIRE(!memcmp(before,web_fb,sizeof before));}
   TSEL->p[id]=old;draw();
  }
 }
 REQUIRE(!bad_rects&&!bad_ink&&!bad_contrast);
#endif
 printf("LFO MOD visuals: %u checks passed; %u unrelated frames; native 240x240 bounds/value sweeps\n",checks,shots);return 0;
}
