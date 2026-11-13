/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <string.h>
static char captured[256][32];static unsigned captured_n,blit_pixels;
static unsigned bad_rects,clipped_text,strip_clips,value_clips,roll_clips;
static void watch_rect(unsigned x,unsigned y,unsigned w,unsigned h){bad_rects+=x+w>240 || y+h>240 || w*h>240u*124u;blit_pixels+=w*h;}
static void watch_text(const char*s,unsigned flags,unsigned w,unsigned h,int y0,int y1){
 if(captured_n<256){unsigned n=0;while(s[n] && n<31){captured[captured_n][n]=s[n];n++;}captured[captured_n++][n]=0;}
 clipped_text+=(flags&2u)!=0;
 if(flags&2u){if(w==57 && h==122)strip_clips++;
 if(w==57 && h==44 && ((*s>='0'&&*s<='9')||*s=='+'||*s=='-'||!strcmp(s,"OFF")||!strcmp(s,"ON"))){if(y0 || y1!=(int)h)roll_clips++;else value_clips++;}}}

#define GFX_HOOK_TEXT(x0,y0,x1,y1,s,flags) watch_text(s,flags,cv_w,cv_h,cv_cy0,cv_cy1)
#define GFX_HOOK_BLIT(x,y,r0) watch_rect(x,y,cv_w,cv_h)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#include "felucca_web.c"
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"MIXER line %u: %s\n",__LINE__,#x);return 1;}}while(0)
static int seen(const char*s){for(unsigned i=0;i<captured_n;i++)if(!strcmp(s,captured[i]))return 1;return 0;}
static void picture(const char *path)
{
 if(!path)return;FILE*f=fopen(path,"wb");if(!f)return;fprintf(f,"P6\n240 240\n255\n");
 for(unsigned i=0;i<240*240;i++){unsigned v=swap16(web_fb[i]);unsigned char rgb[]={((v>>11)&31)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(rgb,3,1,f);}fclose(f);
}
#ifndef MIXER_VISUALS_NO_MAIN
int main(int argc,char **argv)
{
 web_nor_erase();web_boot();while(fm1_ms<3500)web_ms();
 REQUIRE(ui.home && !mixer_visuals_page());home_tap();ui_draw();
 REQUIRE(mixer_visuals_page() && cur_page()->scope==SC_TRK && mixer_visuals.valid);
 static const unsigned ids[]={P_LEVEL,P_PAN,P_REV,P_MUTE};
 const int16_t values[4][4]={{100,-64,0,0},{80,-21,42,0},{0,0,85,1},{127,63,127,0}};
 for(unsigned c=0;c<4;c++)for(unsigned k=0;k<4;k++)trk[c].p[ids[k]]=values[c][k];
 for(unsigned c=0;c<4;c++) {
  song.sel=c;ui.force=1;captured_n=0;ui_draw();
  for(unsigned k=0;k<4;k++){char expected[16];const char *unit;param_format(&TP[ids[k]],trk[c].p[ids[k]],expected,&unit);
#if COFFEE_MIXER_TOP_KNOBS
   char formatted[24];snprintf(formatted,sizeof formatted,"%s%s%s",expected,*unit?" ":"",unit);REQUIRE(seen(formatted));
#else
   REQUIRE(seen(expected));
#endif
  }
  REQUIRE(web_fb[Y_GRAPH*240+(unsigned)CARD_X(c)]==swap16(HOME_ACCENT[c]));
  for(unsigned k=0;k<4;k++) {
   int16_t before=trk[c].p[ids[k]];int direction=before==TP[ids[k]].max?-1:1;
   static int16_t saved[4][P_COUNT];for(unsigned p=0;p<4;p++)memcpy(saved[p],trk[p].p,sizeof saved[p]);
   web_enc(EN_K1+k,direction);ui_input();ui_draw();
   REQUIRE(trk[c].p[ids[k]]!=before);
   for(unsigned p=0;p<4;p++)for(unsigned id=0;id<P_COUNT;id++)if(p!=c || id!=ids[k])REQUIRE(trk[p].p[id]==saved[p][id]);
  }
 }
 /* Real channel encoder, not a portrait hit target or new mapping. */
 web_enc(EN_ALGO,-1);ui_input();ui_draw();REQUIRE(song.sel==2);
 /* Actual peak, pan/send and mute indicators. */
 trk[0].p[P_MUTE]=0;trk[0].peak=32767;ui.force=0;ui_draw();REQUIRE(ts.meter[0]>0);
 trk[0].p[P_MUTE]=1;trk[0].peak=32767;ui.force=1;ui_draw();REQUIRE(ts.meter[0]==0);
 trk[2].peak=32767;ui.force=0;blit_pixels=0;ui_draw();REQUIRE(blit_pixels==84); /* meter only, no portrait/control copy */
 /* LARGE remains stored; the redesigned MIXER keeps compact cards with larger labels. */
 uint32_t prefs=ui_prefs;ui_prefs|=PREF_LARGE;ui.force=1;ui_draw();REQUIRE(large_kind()==LK_LABEL && (ui_prefs&PREF_LARGE));ui_prefs=prefs;
 /* The selected pattern uses beans; clock/other pages keep their existing path. */
 trk[2].step[0].n=1;trk[2].step[0].time=ST_NOTE;trk[2].step[0].note[0]=60;ui.help_t=0;ui.force=1;ui_draw();
 REQUIRE(web_fb[(Y_FOOT+1)*240+(unsigned)bar_x(0)+4]==swap16(HOME_ACCENT[2]));
 if(argc>1)picture(argv[1]);
 home_tap();ui_draw();REQUIRE(!ui.home && !mixer_visuals_page() && cur_page()->scope!=SC_TRK);
 home_tap();ui_draw();REQUIRE(ui.home && !mixer_visuals_page() && scientist_home.valid);
 printf("bounds=%u strip-clips=%u static-value-clips=%u intentional-roll-clips=%u\n",bad_rects,strip_clips,value_clips,roll_clips);
 REQUIRE(!bad_rects && !strip_clips && !value_clips);printf("MIXER visuals: %u checks passed; state %zu B, new bitmap bytes 0, text clips %u\n",checks,sizeof mixer_visuals,clipped_text);
 return 0;
}

#endif
