/* SPDX-License-Identifier: GPL-3.0-only */
/* Optional C1_HOST uses the existing pthread simulator for every part split. */
#ifdef C1_HOST
#include "c1_host.h"
#endif
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"HOME levels line %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    FILE *audio=argc>1?fopen(argv[1],"wb"):0;
    REQUIRE(argc<2 || audio);
    for(unsigned spread=0;spread<2;spread++) for(unsigned part=0;part<4;part++) {
        ui_power_on();
#if FELUCCA_HOME_LEVELS
        memset(home_level_raw,0,sizeof home_level_raw);memset(home_level_env,0,sizeof home_level_env);
        for(unsigned p=0;p<4;p++)home_level_pub[p]=0;
#endif
        for(unsigned p=0;p<4;p++) {
            host_preset(&trk[p],0,5);memset(trk[p].v,0,sizeof trk[p].v);trk[p].tail=0;
            trk[p].p[P_MUTE]=0;trk[p].p[P_LEVEL]=100;trk[p].p[P_ATK]=0;
            trk[p].p[P_SUS]=127;trk[p].p[P_REL]=0;trk[p].p[P_CHOR]=trk[p].p[P_DLY]=trk[p].p[P_REV]=0;
            trk[p].p[P_DIST]=trk[p].p[P_ITYPE]=0;trk[p].p[P_PAN]=(part&1)?63:-64;
            trk[p].p[P_SPRD]=spread?100:0;
        }
        trk_note_on(&trk[part],60,110);
        for(unsigned j=0;j<1000;j++) {
            int32_t out[CTL*2];mix_block(out,CTL);if(audio)fwrite(out,sizeof out,1,audio);
        }
#if FELUCCA_HOME_LEVELS
        for(unsigned p=0;p<4;p++)REQUIRE(p==part?home_level_read(p)>100:home_level_read(p)==0);
        /* Existing meters are read/cleared by UI; the new signal is independent. */
        uint16_t saved=home_level_read(part);trk[part].peak=0;REQUIRE(home_level_read(part)==saved);
#endif
        trk[part].p[P_LEVEL]=0; /* a held note must not animate when its actual output is zero */
        for(unsigned j=0;j<6000;j++) {
            int32_t out[CTL*2];mix_block(out,CTL);if(audio)fwrite(out,sizeof out,1,audio);
        }
        for(unsigned p=0;p<4;p++)REQUIRE(home_level_read(p)==0);
    }
    ui_power_on();kb_boot_hold=0;scrn.st=SCR_ON;
    home_level_view_t view={0};REQUIRE(home_levels_poll(&view,0)==15);
    ui.menu=1;REQUIRE(home_levels_poll(&view,100)==0);ui.menu=0;
    ui.layer=1;REQUIRE(home_levels_poll(&view,200)==0);ui.layer=0;
    ui.confirm=1;REQUIRE(home_levels_poll(&view,300)==0);ui.confirm=0;
    ui.uboot=1;REQUIRE(home_levels_poll(&view,400)==0);ui.uboot=0;
    ui.home=0;REQUIRE(home_levels_poll(&view,500)==0);ui.home=1;
    scrn.st=SCR_OFF;REQUIRE(home_levels_poll(&view,600)==0);scrn.st=SCR_ON;
    kb_boot_hold=1;REQUIRE(home_levels_poll(&view,700)==0);kb_boot_hold=0;
    REQUIRE(home_levels_poll(&view,800)==15);
    if(audio)fclose(audio);
    printf("HOME levels: %u checks passed; CH1..4 actual audio, pan/spread, silent level, HOME-only reader; feature=%u\n",checks,FELUCCA_HOME_LEVELS);
    return 0;
}
