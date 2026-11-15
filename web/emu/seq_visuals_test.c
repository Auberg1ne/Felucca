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
#ifndef SEQ_WEB_SOURCE
#define SEQ_WEB_SOURCE "felucca_web.c"
#endif
#include SEQ_WEB_SOURCE
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"SEQ visuals %u: %s rect%u ink%u contrast%u\n",__LINE__,#x,bad_rects,bad_ink,bad_contrast);return 1;}}while(0)
static void draw(void){text_n=0;ui.help_t=ui.msg_t=0;ui_draw();}
static int seen(const char*s){for(unsigned i=0;i<text_n;i++)if(!strcmp(texts[i],s))return 1;return 0;}
static void picture(const char*dir,const char*name){char p[1024];snprintf(p,sizeof p,"%s/%s.ppm",dir,name);FILE*f=fopen(p,"wb");if(!f)return;
 fprintf(f,"P6\n240 240\n255\n");for(unsigned i=0;i<240*240;i++){unsigned c=swap16(web_fb[i]);unsigned char rgb[]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(rgb,3,1,f);}fclose(f);}
static void populate(unsigned track){song.sel=track;track_defaults_steps(TSEL);TSEL->p[P_SLEN]=16;TSEL->p[P_SQNT]=0;
 for(unsigned i=0;i<16;i++){step_t*s=&TSEL->step[i];s->time=i%5==1?ST_REST:i%5==2?ST_TIE:ST_NOTE;
  if(s->time==ST_NOTE){s->n=1;s->note[0]=48+i*3;s->vel=1+i*8;}s->flags=(i&1?SF_ACCENT:0)|(i%3==0?SF_SLIDE:0);
  ev_sput(s,EVK_CHANCE,i%2?0:100);ev_sput(s,EVK_RATCH,1+i%4);ev_sput(s,EVK_NUDGE,(int)i-8);
 }
 TSEL->step[5].time=ST_TIE;TSEL->step[5].n=0;TSEL->step[10].time=ST_TIE;TSEL->step[10].n=0;
 motion_clear(TSEL);motion_set_event(TSEL,3,P_REV,70);motion_set_lock(TSEL,5,P_E1,80);
}
static void view(unsigned which){ui.home=0;ui.layer=ui.ly=ui.lock=0;fm1_in.buttons=0;
 ui.page=(uint8_t)page_first(FAM_SEQ);if(which<3)ui.page+=which;page_entered();
 if(which>=3){ui.ly=LAYER_SEQ;ui.ly_t0=(fm1_ticks()&~15u)|1u|LY_OPEN;fm1_in.buttons=ly_bit(LAYER_SEQ);layer_opened(LAYER_SEQ);layer_show();if(which==4)eu_open(TSEL);}
 ui.force=1;
}
static void output(FILE*f,const char*dir,const char*name){ui.force=1;draw();fwrite(web_fb,sizeof web_fb,1,f);if(name)picture(dir,name);}
int main(int argc,char**argv){web_nor_erase();web_boot();while(fm1_ms<3500)web_ms();REQUIRE(argc==3);FILE*f=fopen(argv[2],"wb");REQUIRE(f);unsigned shots=0;
 for(unsigned t=0;t<4;t++)for(unsigned i=0;i<NPAGES;i++){song.sel=t;if(!page_visible(i)||PAGES[i].fam==FAM_SEQ)continue;ui.home=0;ui.page=i;page_entered();ui.force=1;draw();fwrite(web_fb,sizeof web_fb,1,f);shots++;}
 for(unsigned t=0;t<4;t++){song.sel=t;go_home();ui.force=1;draw();fwrite(web_fb,sizeof web_fb,1,f);shots++;}
 /* GLO layer shares cards: its pixels must also stay exact. */
 for(unsigned t=0;t<4;t++){song.sel=t;go_home();ui.ly=LAYER_GLO;ui.ly_t0=(fm1_ticks()&~15u)|1u|LY_OPEN;fm1_in.buttons=ly_bit(LAYER_GLO);layer_opened(LAYER_GLO);layer_show();ui.force=1;draw();fwrite(web_fb,sizeof web_fb,1,f);shots++;fm1_in.buttons=0;layer_let_go(0);layer_show();}
 fclose(f);char path[1024];snprintf(path,sizeof path,"%s/targets.bin",argv[1]);f=fopen(path,"wb");REQUIRE(f);
 static const char*names[]={"piano","detail","automation","tools","euclid"};
 for(unsigned t=0;t<4;t++){populate(t);for(unsigned v=0;v<5;v++){view(v);output(f,argv[1],t==0?names[v]:NULL);if(eu.on)eu_close(TSEL,0);}}
 song.playing=1;TSEL->seq_idx=7;view(0);output(f,argv[1],"piano-playing");view(1);output(f,argv[1],"detail-playing");song.playing=0;
 motion_clear(TSEL);memset(TSEL->step,0,sizeof TSEL->step);view(0);output(f,argv[1],"piano-empty");view(2);output(f,argv[1],"automation-empty");fclose(f);
#ifndef SEQ_BASE_CAPTURE
 watch_target=1;ui_prefs|=PREF_ANIM_OFF;static uint16_t before[240*240];
 for(unsigned t=0;t<4;t++)for(unsigned v=0;v<3;v++)for(unsigned playing=0;playing<2;playing++){
  populate(t);view(v);song.playing=playing;
  for(unsigned i=0;i<16;i++){cursor_set(i);TSEL->seq_idx=i;ui.force=1;draw();memcpy(before,web_fb,sizeof before);draw();REQUIRE(!memcmp(before,web_fb,sizeof before));}
 }
 song.playing=0;
 for(unsigned t=0;t<4;t++){populate(t);view(1);
  for(unsigned i=0;i<16;i++){cursor_set(i);ev_sput(&TSEL->step[i],EVK_CHANCE,i%2?0:100);ev_sput(&TSEL->step[i],EVK_RATCH,1+i%4);ev_sput(&TSEL->step[i],EVK_NUDGE,(int)i-8);ev_sput(&TSEL->step[i],EVK_VEL,i%2?1:127);ui.force=1;draw();
   REQUIRE(seen("CHANCE")&&seen("RATCH")&&seen("NUDGE")&&seen("VEL"));}
  view(2);uint16_t rows[EV_ROWS];unsigned n=ev_list(rows);
  for(unsigned row=0;row<n;row++){ui.ev_row=row;ui.force=1;draw();memcpy(before,web_fb,sizeof before);draw();REQUIRE(!memcmp(before,web_fb,sizeof before));}
  for(unsigned pitch=0;pitch<128;pitch++){view(0);TSEL->step[0].time=ST_NOTE;TSEL->step[0].n=1;TSEL->step[0].note[0]=pitch;ui.cursor=0;ui.force=1;draw();memcpy(before,web_fb,sizeof before);draw();REQUIRE(!memcmp(before,web_fb,sizeof before));}
  for(unsigned len=1;len<=64;len++){view(4);eu.steps=len;eu.hits=len/2;eu.rot=len-1;ui.force=1;draw();REQUIRE(seen("HITS")&&seen("STEPS")&&seen("ROT"));eu_close(TSEL,0);}
 }
 for(unsigned cond=101;cond<=100+COND_NFILL;cond++){
  populate(0);TSEL->step[0].time=ST_NOTE;TSEL->step[0].n=1;ev_sput(&TSEL->step[0],EVK_CHANCE,cond);view(1);ui.cursor=0;ui.force=1;draw();
  REQUIRE(seen("COND"));view(2);ui.force=1;draw();
 }
 populate(0);view(3);fm1_in.notes=0;draw();
 for(unsigned p=0;p<TL_N;p++)if(tl_cell(TSEL,p)){
  unsigned key=0;while(key<27 && (key_black(key)||key_place(key)!=p))key++;REQUIRE(key<27);
  unsigned x=p<TL_DOUBLE?LC_X(p%4)+LC_W/2:168+35*(p-TL_DOUBLE)+15;
  unsigned y=p<TL_DOUBLE?4+32*(p/4)+2:102;
  fm1_in.notes=1u<<key;draw();REQUIRE(web_fb[(Y_GRAPH+y)*240+x]==swap16(0x18c4));
  fm1_in.notes=0;draw();REQUIRE(web_fb[(Y_GRAPH+y)*240+x]==swap16(0x0841));
 }
 REQUIRE(!bad_ink&&!bad_contrast);
#endif
 REQUIRE(!bad_rects);printf("SEQ visuals: %u assertions passed; %u unrelated frames; native bounds and extremes\n",checks,shots);return 0;}
