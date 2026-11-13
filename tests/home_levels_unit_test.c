/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "../firmware/src/home_levels.h"
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"home level unit line %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    REQUIRE(home_level_peak(0,INT_MIN,INT_MAX)==32767);
    REQUIRE(home_level_peak(0,16000,-16000)==16000); /* stereo cancellation must not hide output */
    REQUIRE(home_level_peak(100,0,-10)==100);
    REQUIRE(home_level_read(4)==0 && home_level_read(UINT32_MAX)==0);
    REQUIRE(sizeof home_level_raw+sizeof home_level_env+sizeof home_level_pub==48);
    REQUIRE(((uintptr_t)home_level_pub&3u)==0);
    unsigned target[]={0,100,4000,16000};
    for(unsigned i=0;i<4;i++)home_level_end(i,target[i]);
    for(unsigned n=0;n<2000;n++) {
        uint16_t before=home_level_read(3);home_level_publish(16);
        REQUIRE(home_level_read(3)>=before && home_level_read(3)<=16000);
    }
    for(unsigned p=0;p<4;p++)REQUIRE(home_level_read(p)==target[p] && home_level_band(home_level_read(p))==p);
    uint16_t high=home_level_read(3);home_level_begin(3);home_level_publish(16);
    REQUIRE(home_level_read(3)<high && home_level_read(3)>high*99u/100u);
    home_level_view_t view={0};
    REQUIRE(home_level_frame(&view,0,1)==15);
    REQUIRE(home_level_frame(&view,49,1)==0 && view.last_ms==0);
    REQUIRE(home_level_frame(&view,50,1)==14);
    REQUIRE(home_level_frame(&view,100,0)==0 && !view.valid);
    REQUIRE(home_level_frame(&view,101,1)==15);
    view.last_ms=UINT32_MAX-20u;view.valid=1;
    REQUIRE(home_level_frame(&view,view.last_ms+49u,1)==0);
    REQUIRE(home_level_frame(&view,29,1)==14 && view.last_ms==29);
    for(unsigned p=0;p<4;p++)home_level_begin(p);
    for(unsigned n=0;n<10000;n++)home_level_publish(16);
    for(unsigned p=0;p<4;p++)REQUIRE(home_level_read(p)==0);
    REQUIRE(home_level_frame(&view,100,1)==14); /* one redraw to rest */
    REQUIRE(home_level_frame(&view,150,1)==0); /* no redraw at silent rest */
    for(unsigned v=0;v<32768;v++)REQUIRE(v==0 || home_level_band(v)>=home_level_band(v-1));
    printf("HOME level unit: %u checks passed; smoothing, stereo extremes, rest, bands, rate, wrap and bounds; view %zu B\n",checks,sizeof view);
    return 0;
}
