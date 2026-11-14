/* SPDX-License-Identifier: GPL-3.0-only */
/* Compare musical state and every non-menu pixel against the base checkout. */
#define UI_TEST_NO_MAIN 1
#ifndef SETTINGS_UI_SOURCE
#define SETTINGS_UI_SOURCE "ui_test.c"
#endif
#include SETTINGS_UI_SOURCE
static unsigned snapshots;
static void snapshot(FILE *f)
{
    uint32_t state[]={song.sel,ui.home,ui.page,ui.menu,ui.layer,ui.confirm,song.playing};
    fwrite(state,sizeof state,1,f);
    for(unsigned p=0;p<4;p++)fwrite(trk[p].p,sizeof trk[p].p,1,f);
    if(!ui.menu)fwrite(host_screen,sizeof host_screen,1,f);
    fwrite(&ui_prefs,sizeof ui_prefs,1,f);fwrite(&ui_prefs2,sizeof ui_prefs2,1,f);
    fwrite(song.g,sizeof song.g,1,f);
    snapshots++;
}
int main(int argc,char **argv)
{
    if(argc!=2)return 2;FILE *f=fopen(argv[1],"wb");if(!f)return 2;
    ui_power_on();kb_boot_hold=0;frame();snapshot(f);
    for(unsigned k=0;k<8;k++) {
        turn(EN_SELECT,1);snapshot(f);
        for(unsigned role=EN_K1;role<=EN_K4;role++){turn(role,1);snapshot(f);turn(role,-1);snapshot(f);}
    }
    /* Explicitly traverse HOME -> MIXER -> CLOCK -> HOME in normal and LARGE
     * modes. MIXER pixels may change; every parameter and other pixel must not. */
    uint32_t prefs=ui_prefs;
    for(unsigned large=0;large<2;large++) {
        if(large)ui_prefs|=PREF_LARGE;else ui_prefs&=~PREF_LARGE;
        press(B_HOME);snapshot(f);
        for(unsigned role=0;role<NE;role++){turn(role,1);snapshot(f);turn(role,-1);snapshot(f);}
        press(B_HOME);snapshot(f);
        for(unsigned role=0;role<NE;role++){turn(role,1);snapshot(f);turn(role,-1);snapshot(f);}
        press(B_HOME);snapshot(f);
    }
    ui_prefs=prefs;
    const unsigned buttons[]={B_EDIT,B_ENV,B_LFO,B_FX,B_SCL,B_SEQ,B_GLO,B_SAVE,B_HOME};
    for(unsigned i=0;i<NELEM(buttons);i++) {
        press(buttons[i]);snapshot(f);
        for(unsigned role=0;role<NE;role++){turn(role,1);snapshot(f);turn(role,-1);snapshot(f);}
    }
    fclose(f);printf("Settings navigation: %u snapshots; all non-menu pixels recorded\n",snapshots);return 0;
}
