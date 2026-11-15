/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned checks,bad_rects,seen_label,bad_label;
static int label_x0,label_x1,label_y0,label_y1,preset_right;
static void text_watch(int x0,int y0,int x1,int y1,const char*s,unsigned flags,unsigned w,unsigned h){
 if(w==240 && h==38 && s[0]=='W')preset_right=x1;
 if(!strcmp(s,"MOD MATRIX") && w==240 && h==38){seen_label++;label_x0=x0;label_x1=x1;label_y0=y0;label_y1=y1;bad_label+=flags!=0 || x0<0 || x1>232 || y0<0 || y1>38;}
}
static void bounds(unsigned x,unsigned y,unsigned w,unsigned h){bad_rects+=x+w>240 || y+h>240;}
#define GFX_HOOK_TEXT(x0,y0,x1,y1,s,flags) text_watch(x0,y0,x1,y1,s,flags,cv_w,cv_h)
#define GFX_HOOK_BLIT(x,y,r0) bounds(x,y,cv_w,cv_h)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#ifndef MOD_LABEL_WEB_SOURCE
#define MOD_LABEL_WEB_SOURCE "felucca_web.c"
#endif
#include MOD_LABEL_WEB_SOURCE
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"MOD label %u: %s\n",__LINE__,#x);return 1;}}while(0)
static void draw(void){ui.help_t=ui.msg_t=0;ui.force=1;ui_draw();}
static int picture(const char*dir){char p[1024];snprintf(p,sizeof p,"%s/mod-matrix.ppm",dir);FILE*f=fopen(p,"wb");if(!f)return 1;
 fprintf(f,"P6\n240 240\n255\n");for(unsigned i=0;i<240*240;i++){unsigned c=swap16(web_fb[i]);unsigned char rgb[]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(rgb,3,1,f);}return fclose(f)!=0;}
int main(int argc,char**argv){
 web_nor_erase();web_boot();while(fm1_ms<3500)web_ms();REQUIRE(argc==3);FILE*f=fopen(argv[2],"wb");REQUIRE(f);unsigned frames=0;
 /* All unrelated pages, unchanged pixels. */
 for(unsigned ch=0;ch<4;ch++)for(unsigned i=0;i<NPAGES;i++){
  song.sel=ch;if(!page_visible(i)||PAGES[i].graph==GR_MOD)continue;
  ui.home=0;ui.page=i;page_entered();draw();fwrite(web_fb,sizeof web_fb,1,f);frames++;
 }
 for(unsigned ch=0;ch<4;ch++){song.sel=ch;go_home();draw();fwrite(web_fb,sizeof web_fb,1,f);frames++;}
 long unrelated=ftell(f);song.sel=0;open_family(FAM_SCL);
 for(unsigned ch=0;ch<4;ch++)for(unsigned slot=0;slot<4;slot++){
  song.sel=ch;mod_ui_slot=slot;unsigned old_seen=seen_label;draw();fwrite(web_fb,sizeof web_fb,1,f);frames++;
  char title[20];page_title(title);REQUIRE(!strcmp(title,"MOD"));
#ifndef MOD_LABEL_BASE_CAPTURE
  REQUIRE(seen_label==old_seen+1 && !bad_label);int32_t ink[4];text_ink(&AF_S,"MOD MATRIX",ink);
  REQUIRE(label_x0==232-text_w(&AF_S,"MOD MATRIX")+ink[0] && label_x1==232-text_w(&AF_S,"MOD MATRIX")+ink[2] && label_x1<=232);
  REQUIRE(label_y0>=19 && label_y1<=38);
#endif
 }
 song.sel=0;mod_ui_slot=0;draw();REQUIRE(!picture(argv[1]));
#ifndef MOD_LABEL_BASE_CAPTURE
 /* Max-length names use the unchanged bounded preset-name renderer. */
 int rc=up_store(0,"WWWWWWWWWWWWWWW");REQUIRE(rc==0||rc==3);REQUIRE(!up_load(0));draw();
 REQUIRE(seen_label&&!bad_label && preset_right>0 && preset_right<=label_x0-12);
 static uint16_t before[240*240];memcpy(before,web_fb,sizeof before);ui_draw();REQUIRE(!memcmp(before,web_fb,sizeof before));
#endif
 REQUIRE(!fclose(f)&&!bad_rects);
 printf("MOD footer: %u checks passed; %u frames, %ld unrelated bytes; label bounds x%d..%d y%d..%d\n",checks,frames,unrelated,label_x0,label_x1,label_y0,label_y1);return 0;
}
