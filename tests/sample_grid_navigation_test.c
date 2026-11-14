/* SPDX-License-Identifier: GPL-3.0-only */
/* Compile unchanged against exact base/current UI sources, compare all output. */
#define UI_TEST_NO_MAIN 1
#ifndef GRID_UI_SOURCE
#define GRID_UI_SOURCE "ui_test.c"
#endif
#include GRID_UI_SOURCE
static unsigned snapshots;
static void shot(FILE*f){uint32_t state[]={song.sel,ui.home,ui.page,ui.menu,ui.layer,ui.confirm,song.playing};
 fwrite(state,sizeof state,1,f);fwrite(host_screen,sizeof host_screen,1,f);
 for(unsigned t=0;t<NTRK;t++)fwrite(trk[t].p,sizeof trk[t].p,1,f);
 fwrite(song.g,sizeof song.g,1,f);fwrite(&ui_prefs,sizeof ui_prefs,1,f);fwrite(&ui_prefs2,sizeof ui_prefs2,1,f);snapshots++;}
int main(int argc,char**argv){
 if(argc!=2)return 2;FILE*f=fopen(argv[1],"wb");if(!f)return 2;
 for(unsigned e=0;e<NENGINES;e++)if(eng_ok(e)){
  for(unsigned page=0;page<NPAGES;page++){
   ui_power_on();set_engine_of(TSEL,e);TSEL->engine=e;ui.home=0;ui.page=page;
   if((e==ENGI_SAMPLE && PAGES[page].fam==FAM_EDIT) || !page_visible(page))continue;
   page_entered();frame();shot(f);
   unsigned rows=e==ENGI_GRAIN && PAGES[page].fam==FAM_EDIT?3:1;
   for(unsigned r=0;r<rows;r++){
    for(unsigned c=0;c<4;c++){turn(EN_K1+c,1);shot(f);turn(EN_K1+c,-1);shot(f);}
    if(rows>1){turn(EN_SELECT,1);shot(f);}
   }
  }
  ui_power_on();set_engine_of(TSEL,e);TSEL->engine=e;frame();shot(f);
  for(unsigned i=0;i<3;i++){press(B_HOME);shot(f);}
 }
 for(unsigned tab=0;tab<MTAB_COUNT;tab++){
  ui_power_on();ui.menu=1;ui.menu_sel=menu_visible_row(tab,0);ui.force=1;frame();shot(f);
 }
 fclose(f);printf("SAMPLE isolation: %u non-SAMPLE-EDIT state/pixel snapshots, including all GRAIN rows\n",snapshots);return 0;
}
