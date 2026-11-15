/* SPDX-License-Identifier: GPL-3.0-only */
/* No rendering/audio: exercise the exact gesture path with every UBSan check on. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define REQUIRE(x) do { checks++;if(!(x)){fprintf(stderr,"EDIT timing %u: %s\n",__LINE__,#x);return 1;} }while(0)
int main(void)
{
    const uint32_t starts[]={16u,100000u,0xffff0000u};
    for(unsigned track=0;track<NTRK;track++)for(unsigned h=0;h<4;h++)for(unsigned st=0;st<NELEM(starts);st++)for(int edge=-1;edge<3;edge++)for(unsigned late=0;late<2;late++) {
        ui_power_on();memset(&lys,0,sizeof lys);song.sel=track;settings_hold=h;
        uint32_t start=starts[st],limit=(uint32_t)HOLD_MS[h]*1000u*FM1_TICKS_PER_US;
        uint32_t when=start+limit+(edge==2?1000000:edge);
        fm1_in.buttons=ly_bit(LAYER_EDIT);layer_arm(ly_bit(LAYER_EDIT),start);
        uint8_t fav[sizeof favorites];memcpy(fav,&favorites,sizeof fav);
        REQUIRE(ui.ly==LAYER_EDIT);
        if(!late){REQUIRE(!layer_gesture(when,0));REQUIRE(!layer_open() && !ui.layer);}
        fm1_in.buttons=0;unsigned tap=layer_gesture(when,0);
        REQUIRE(tap==(edge<0?LAYER_EDIT:0));REQUIRE(!ui.ly && !ui.lock && !ui.ly_t0);
        REQUIRE(!memcmp(fav,&favorites,sizeof fav));
    }
    printf("Long EDIT strict timing: %u checks passed; threshold, late release and uint32 wrap; no sanitizer exclusions\n",checks);
    return 0;
}
