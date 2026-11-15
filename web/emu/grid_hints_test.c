/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned checks,bad_rects;
static void bounds(unsigned x,unsigned y,unsigned w,unsigned h){bad_rects+=x+w>240 || y+h>240;}
#define GFX_HOOK_BLIT(x,y,r0) bounds(x,y,cv_w,cv_h)
#ifndef GRID_HINTS_WEB_SOURCE
#define GRID_HINTS_WEB_SOURCE "felucca_web.c"
#endif
#include GRID_HINTS_WEB_SOURCE
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"Grid hints %u: %s\n",__LINE__,#x);return 1;}}while(0)
static void draw(void){ui.help_t=ui.msg_t=0;ui_draw();}
static int capture(const char*dir,unsigned ch,unsigned page,unsigned row){char path[1024];snprintf(path,sizeof path,"%s/ch%u-page%u-row%u.ppm",dir,ch+1,page+1,row+1);FILE*f=fopen(path,"wb");if(!f)return 1;
 fprintf(f,"P6\n240 240\n255\n");for(unsigned i=0;i<240*240;i++){unsigned v=swap16(web_fb[i]);unsigned char rgb[]={((v>>11)&31)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(rgb,3,1,f);}return fclose(f)!=0;}
static int clean(void){
#ifndef GRID_HINTS_BASE_CAPTURE
 for(unsigned y=188;y<200;y++)for(unsigned x=0;x<240;x++)REQUIRE(web_fb[y*240+x]==0);
#endif
 return 0;
}
int main(int argc,char**argv){
 web_nor_erase();web_boot();while(fm1_ms<3500)web_ms();
 REQUIRE(argc==3);FILE*f=fopen(argv[2],"wb");REQUIRE(f);
 unsigned frames=0;
 for(unsigned ch=0;ch<4;ch++)for(unsigned page=0;page<3;page++) {
  song.sel=ch;ui.home=0;ui.page=page==2?page_first(FAM_ENV):page_first(FAM_EDIT)+page;page_entered();
  TSEL->step[0].n=1;TSEL->step[0].note[0]=60;TSEL->step[1].time=ST_TIE;TSEL->step[2].time=ST_REST;
  for(unsigned row=0;row<(page==2?2u:3u);row++) {
   grain_edit_row=row;ui.force=1;draw();if(clean())return 1;
   REQUIRE(!capture(argv[1],ch,page,row));fwrite(web_fb,sizeof web_fb,1,f);frames++;
   /* Incremental parameter changes and full redraw must agree, hint region stays clean. */
   for(unsigned col=0;col<4;col++) {
    unsigned id=page_param_id(cur_page(),col);if(id>=P_COUNT)continue;
    int old=TSEL->p[id];const param_desc_t*d=track_desc(TSEL,id);
    TSEL->p[id]=old==d->min?d->max:d->min;draw();if(clean())return 1;
    static uint16_t before[240*240];memcpy(before,web_fb,sizeof before);ui.force=1;draw();
    REQUIRE(!memcmp(before,web_fb,sizeof before));if(clean())return 1;
    TSEL->p[id]=old;draw();
   }
#ifndef GRID_HINTS_BASE_CAPTURE
   /* Old text left by any preceding screen is cleared on a full redraw. */
   lcd_fill(0,188,240,12,0xffff);ui.force=1;draw();if(clean())return 1;
#endif
  }
 }
 /* Also compare untouched screens, complete pixels without any allowed region. */
 for(unsigned ch=0;ch<4;ch++)for(unsigned i=0;i<NPAGES;i++) {
  song.sel=ch;if(!page_visible(i) || parameter_grid_rows(&PAGES[i]))continue;
  ui.home=0;ui.page=i;page_entered();ui.force=1;draw();fwrite(web_fb,sizeof web_fb,1,f);frames++;
 }
 for(unsigned ch=0;ch<4;ch++){song.sel=ch;go_home();ui.force=1;draw();fwrite(web_fb,sizeof web_fb,1,f);frames++;}
 REQUIRE(!fclose(f) && !bad_rects);
 printf("Grid hints: %u checks passed, %u capture frames (32 grid states), 240x240 bounds\n",checks,frames);return 0;
}
