/* SPDX-License-Identifier: GPL-3.0-only */
/* Build with COFFEE_HOME_VISUALS=0/1; compare parameter/navigation traces and
 * every non-HOME framebuffer. This never asserts obsolete HOME pixel goldens. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned snapshots;
static void snapshot(FILE *f)
{
    uint32_t state[]={song.sel,ui.home,ui.page,ui.menu,ui.layer,ui.confirm,song.playing};
    fwrite(state,sizeof state,1,f);
    for(unsigned p=0;p<4;p++)fwrite(trk[p].p,sizeof trk[p].p,1,f);
    if(!ui.home || ui.menu || ui.layer || ui.confirm)fwrite(host_screen,sizeof host_screen,1,f);
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
    const unsigned buttons[]={B_EDIT,B_ENV,B_LFO,B_FX,B_SCL,B_SEQ,B_GLO,B_SAVE,B_HOME};
    for(unsigned i=0;i<NELEM(buttons);i++) {
        press(buttons[i]);snapshot(f);
        for(unsigned role=0;role<NE;role++){turn(role,1);snapshot(f);turn(role,-1);snapshot(f);}
    }
    fclose(f);printf("HOME navigation: %u snapshots; knobs/selection and non-HOME pixels recorded; visuals=%u\n",snapshots,COFFEE_HOME_VISUALS);return 0;
}
