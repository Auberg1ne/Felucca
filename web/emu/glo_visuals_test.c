/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned checks,bad_rects,bad_ink,text_n,blits,pixels,ink_luma,bad_contrast;
static char texts[128][32];
static unsigned watch_target;
static void grid_watch_ink(int x0,int y0,int x1,int y1,const char*s,unsigned flags,unsigned w,unsigned h,const uint16_t *px){
 if(text_n<128)snprintf(texts[text_n++],32,"%s",s);
 if(watch_target && ((w==57 && (h==44 || h==50)) || (w==999 && h==26))){
  unsigned brightest=0;
  for(int y=y0<0?0:y0;y<y1 && y<(int)h;y++)for(int x=x0<0?0:x0;x<x1 && x<(int)w;x++){
   unsigned v=px[y*w+x],c=(v>>8)|((v&255)<<8);
   unsigned l=((c>>11)*255u/31u*54u+((c>>5)&63)*255u/63u*183u+(c&31)*255u/31u*19u)>>8;
   if(l>brightest)brightest=l;
  }
  /* The unchanged dim empty marker peaks at 94 on the light-palette AA curve. */
  if(ink_luma<100 || brightest<((!strcmp(s,"--") || ink_luma<150u)?90u:150u)){bad_contrast++;fprintf(stderr,"contrast %s: ink%u brightest%u\n",s,ink_luma,brightest);}
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
#ifndef GLO_WEB_SOURCE
#define GLO_WEB_SOURCE "felucca_web.c"
#endif
#include GLO_WEB_SOURCE
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"GLO visuals %u: %s rect%u ink%u contrast%u\n",__LINE__,#x,bad_rects,bad_ink,bad_contrast);return 1;}}while(0)
static void draw(void){text_n=0;ui.help_t=ui.msg_t=0;ui_draw();}
static int seen(const char*s){for(unsigned i=0;i<text_n;i++)if(!strcmp(texts[i],s))return 1;return 0;}
static void picture(const char*dir,const char*name){char p[1024];snprintf(p,sizeof p,"%s/%s.ppm",dir,name);FILE*f=fopen(p,"wb");if(!f)return;
 fprintf(f,"P6\n240 240\n255\n");for(unsigned i=0;i<240*240;i++){unsigned c=swap16(web_fb[i]);unsigned char rgb[]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(rgb,3,1,f);}fclose(f);}
static int target(FILE*f,const char*dir,const char*name){ui.force=1;draw();fwrite(web_fb,sizeof web_fb,1,f);if(name)picture(dir,name);return 0;}
int main(int argc,char**argv){
 web_nor_erase();web_boot();while(fm1_ms<3500)web_ms();REQUIRE(argc==3);FILE*f=fopen(argv[2],"wb");REQUIRE(f);unsigned shots=0;
 for(unsigned ch=0;ch<4;ch++)for(unsigned i=0;i<NPAGES;i++){
  song.sel=ch;if(!page_visible(i)||PAGES[i].graph==GR_SONG)continue;ui.page=i;ui.home=0;page_entered();ui.force=1;draw();fwrite(web_fb,sizeof web_fb,1,f);shots++;
 }
 for(unsigned ch=0;ch<4;ch++){song.sel=ch;go_home();ui.force=1;draw();fwrite(web_fb,sizeof web_fb,1,f);shots++;}
 fclose(f);char path[1024];snprintf(path,sizeof path,"%s/targets.bin",argv[1]);f=fopen(path,"wb");REQUIRE(f);
 open_family(FAM_GLO);target(f,argv[1],"song-empty");
 for(unsigned slot=0;slot<4;slot++){for(unsigned t=0;t<4;t++){trk[t].p[P_SLEN]=8+4*slot;memset(trk[t].step,0,sizeof trk[t].step);for(unsigned n=0;n<16;n+=slot+1){trk[t].step[n].n=1;trk[t].step[n].note[0]=60+t;trk[t].step[n].vel=100;}}REQUIRE(!project_save(slot));}
 chain_config.count=4;for(unsigned row=0;row<4;row++){chain_config.row[row].repeat=row+1;for(unsigned t=0;t<4;t++)chain_config.row[row].slot[t]=(row+t)%5<4?(row+t)%5:CHAIN_SILENT;}
 for(unsigned t=0;t<4;t++){song.sel=t;ui.song_row=t;ui.song_trk=t;target(f,argv[1],t==0?"song-populated":NULL);}
 chain.running=1;chain.config=chain_config;chain.row=1;chain.remaining=2;chain.clk=0;song.playing=1;trk[0].seq_idx=4;trk[0].seq_pos=0;
 target(f,argv[1],"song-playing");chain.running=0;song.playing=0;
 /* Open the real overlay, preserving existing map/key positions and footers. */
 go_home();ui.ly=LAYER_GLO;ui.ly_t0=(fm1_ticks()&~15u)|1u|LY_OPEN;fm1_in.buttons=ly_bit(LAYER_GLO);layer_opened(LAYER_GLO);layer_show();
 song.sel=0;target(f,argv[1],"glo-idle");
 for(unsigned t=0;t<4;t++)trk[t].p[P_MUTE]=t&1;perf_solo=2;seq_fill=1;ui.ml.on=1;fm1_in.notes=1u<<7;
 target(f,argv[1],"glo-active");ui.ml.on=0;seq_fill=0;perf_solo=0;fm1_in.notes=0;
 ui.lock=LAYER_GLO;fm1_in.buttons=0;target(f,argv[1],"glo-locked");song.g[G_CLOCK]=1;target(f,argv[1],"glo-external");
 fclose(f);ui.lock=0;fm1_in.buttons=ly_bit(LAYER_GLO);song.g[G_CLOCK]=0;
#ifndef GLO_BASE_CAPTURE
 watch_target=1;
 static uint16_t before[240*240];
 for(unsigned value=0;value<128;value++){
  for(unsigned t=0;t<4;t++)trk[t].p[P_LEVEL]=(value+t*31)%128;
  ui.force=1;draw();for(unsigned t=0;t<4;t++){char b[24];const char*u;param_format(&TP[P_LEVEL],trk[t].p[P_LEVEL],b,&u);if(*u){strcat(b," ");strcat(b,u);}REQUIRE(seen(b));}
  memcpy(before,web_fb,sizeof before);ui_draw();REQUIRE(!memcmp(before,web_fb,sizeof before));
 }
 for(unsigned mask=0;mask<16;mask++)for(unsigned solo=0;solo<16;solo++){
  for(unsigned t=0;t<4;t++)trk[t].p[P_MUTE]=(mask>>t)&1;perf_solo=solo;seq_fill=mask&1;ui.ml.on=(mask>>1)&1;song.g[G_CLOCK]=(mask>>2)&1;fm1_in.notes=((mask&4)?1u<<7:0u)|((mask&8)?1u<<12:0u);
  draw();
  for(unsigned t=0;t<4;t++){
   REQUIRE(web_fb[(Y_GRAPH+4)*240+LC_X(t)+LC_W/2]==swap16((solo&(1u<<t))?GRAIN_ACCENT[t]:GRAIN_COFFEE));
   REQUIRE(web_fb[(Y_GRAPH+LM_Y)*240+LC_X(t)+LC_W/2]==swap16((mask&(1u<<t))?GRAIN_ACCENT[t]:GRAIN_COFFEE));
  }
  REQUIRE(web_fb[(Y_GRAPH+50)*240+LC_X(0)+LC_W/2]==swap16((mask&4)?GRAIN_ACCENT[0]:GRAIN_COFFEE));
  REQUIRE(web_fb[(Y_GRAPH+50)*240+LC_X(3)+LC_W/2]==swap16((mask&8)?GRAIN_ACCENT[3]:GRAIN_COFFEE));
  memcpy(before,web_fb,sizeof before);ui.force=1;draw();REQUIRE(!memcmp(before,web_fb,sizeof before));
 }
 ui.ml.on=0;seq_fill=perf_solo=0;fm1_in.notes=0;fm1_in.buttons=0;ui.ly=ui.layer=0;open_family(FAM_GLO);
 for(unsigned t=0;t<4;t++)for(unsigned row=0;row<=4;row++){
  ui.song_trk=t;ui.song_row=row;ui.force=1;draw();REQUIRE(seen("SEC")&&seen("TRACK")&&seen("PAT")&&seen("REPS"));
  memcpy(before,web_fb,sizeof before);draw();REQUIRE(!memcmp(before,web_fb,sizeof before));
 }
 for(unsigned n=0;n<2;n++){
  chain_config.count=n?CHAIN_ROWS:0;
  for(unsigned r=0;r<CHAIN_ROWS;r++){chain_config.row[r].repeat=1+r%8;for(unsigned t=0;t<4;t++)chain_config.row[r].slot[t]=(r+t)%4;}
  for(unsigned t=0;t<4;t++)for(unsigned r=0;r<=chain_config.count;r++){
   ui.song_trk=t;ui.song_row=r;ui.force=1;draw();memcpy(before,web_fb,sizeof before);draw();REQUIRE(!memcmp(before,web_fb,sizeof before));
  }
 }
 /* Value/style cache fields must not alias (SEC 1 + style 10 vs SEC 11 + style 0). */
 chain_config.count=CHAIN_ROWS;ui.song_row=0;ui.song_trk=2;ui.hot_col=0;ui.hot_t=5;ui.force=1;draw();
 ui.song_row=10;ui.song_trk=0;ui.hot_t=0;draw();memcpy(before,web_fb,sizeof before);ui.force=1;draw();
 /* The existing timeline may still be scrolling; this assertion targets the card cache. */
 REQUIRE(!memcmp(before+Y_LABEL*240,web_fb+Y_LABEL*240,CARD_H*480));
 REQUIRE(!bad_ink&&!bad_contrast);
#endif
 REQUIRE(!bad_rects);printf("GLO visuals: %u assertions passed; %u unrelated frames; bounds and state sweeps\n",checks,shots);return 0;
}
