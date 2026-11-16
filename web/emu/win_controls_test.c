/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned bounds;
static void rect(unsigned x,unsigned y,unsigned w,unsigned h){if(x+w>240||y+h>240)bounds++;}
#define GFX_HOOK_RECT(x,y,w,h) rect(x,y,w,h)
#ifndef WIN_CONTROLS_EMU_SOURCE
#define WIN_CONTROLS_EMU_SOURCE "felucca_web.c"
#endif
#include WIN_CONTROLS_EMU_SOURCE
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"WIN native visual %u: %s\n",__LINE__,#x);return 1;}}while(0)
static void ppm(const char*dir,const char*name){char path[512];snprintf(path,sizeof path,"%s/%s.ppm",dir,name);FILE*f=fopen(path,"wb");if(!f)exit(2);fprintf(f,"P6\n240 240\n255\n");for(unsigned i=0;i<240*240;i++){unsigned v=swap16(web_fb[i]);uint8_t rgb[]={((v>>11)&31)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(rgb,3,1,f);}fclose(f);}
int main(int argc,char**argv){if(argc!=3)return 2;FILE*f=fopen(argv[1],"wb");if(!f)return 2;web_nor_erase();web_boot();while(fm1_ms<3500)web_ms();unsigned frames=0;
 const unsigned values[]={0,1,32,63,64,65,96,126,127};
 for(unsigned ch=0;ch<4;ch++)for(unsigned page=1;page<=2;page++)for(unsigned row=0;row<3;row++)for(unsigned k=0;k<(page==1?9u:1u);k++){
  track_select(ch);ui.home=0;for(unsigned i=0;i<NPAGES;i++)if(grain_grid_page(&PAGES[i])==page)ui.page=i;page_entered();grain_edit_row=row;TSEL->p[109]=values[k];ui.force=1;ui_draw();
  unsigned head[]={ch,page,row,values[k]};fwrite(head,sizeof head,1,f);fwrite(web_fb,sizeof web_fb,1,f);frames++;
  if(ch==0&&row==2&&((page==1&&(values[k]==0||values[k]==64||values[k]==96||values[k]==127))||page==2)){char name[48];snprintf(name,sizeof name,"page%u-win%u",page,values[k]);ppm(argv[2],name);}
 }
 fclose(f);CHECK(!bounds);
#ifdef WIN_CONTROLS_CURRENT
 track_select(0);open_family(FAM_EDIT);grain_edit_row=2;CHECK(page_param_id(cur_page(),1)==109);
 for(unsigned value=0;value<=127;value++){
  TSEL->p[109]=value;ui_draw();uint16_t dirty[240*240];memcpy(dirty,web_fb,sizeof dirty);ui.force=1;ui_draw();CHECK(!memcmp(dirty,web_fb,sizeof dirty));
 }
 /* The simulator's ordinary encoder export drives the real firmware UI. */
 TSEL->p[109]=64;web_enc(EN_K2,1);for(unsigned i=0;i<30;i++)web_ms();CHECK(TSEL->p[109]==65);
 for(unsigned ch=0;ch<4;ch++){
  track_select(ch);go_home();open_family(FAM_EDIT);if(grain_grid_page(cur_page())!=1)open_family(FAM_EDIT);web_enc(EN_SELECT,2);for(unsigned i=0;i<30;i++)web_ms();CHECK(grain_edit_row==2);
  web_enc(EN_K2,-127);for(unsigned i=0;i<30;i++)web_ms();CHECK(trk[ch].p[109]==0);
  web_enc(EN_K2,64);for(unsigned i=0;i<30;i++)web_ms();CHECK(trk[ch].p[109]==64);
  web_enc(EN_K2,63);for(unsigned i=0;i<30;i++)web_ms();CHECK(trk[ch].p[109]==127);
 }
 CHECK(sizeof(gr_grain_t)==64 && sizeof(gr_part_t)==7296 && sizeof(track_t)==1840);
#endif
 printf("WIN native captures: %u frames, %u checks passed; bounds and dirty redraws\n",frames,checks);return 0;
}
