/* SPDX-License-Identifier: GPL-3.0-only */
#define COFFEE_HOME_VISUALS 1
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"HOME steps line %u: %s\n",__LINE__,#x);return 1;}}while(0)
static uint16_t pixel(unsigned step,unsigned x,unsigned y)
{return host_screen[(Y_FOOT+y)*240u+(uint32_t)bar_x(step)+x];}
static void refresh(void){ui.help_t=0;ui.ml.on=0;draw_foot();}
static void blank(track_t *t)
{memset(t->step,0,sizeof t->step);t->p[P_SLEN]=16;}
static int bean(unsigned i){return pixel(i,4,1)==swap16(home_step_accent(song.sel));}
int main(void)
{
    ui_power_on();kb_boot_hold=0;ui.help_t=0;ui.force=1;
    for(unsigned p=0;p<4;p++)blank(&trk[p]);ui_draw();ui.force=0;
    static uint16_t before[240*240];memcpy(before,host_screen,sizeof before);
    for(unsigned i=0;i<16;i++){REQUIRE(!bean(i));REQUIRE(pixel(i,4,10)==swap16(T_MID));}
    for(unsigned i=0;i<16;i++){trk[0].step[i].n=1;trk[0].step[i].note[0]=60;}
    refresh();
    for(unsigned i=0;i<16;i++) {
        REQUIRE(bean(i));REQUIRE(pixel(i,5,6)==swap16(T_BG)); /* diagonal curved groove */
    }
    REQUIRE(!memcmp(before,host_screen,Y_FOOT*240u*2u)); /* scientists, selected frame and top knobs unchanged */
    REQUIRE(!memcmp(before+220*240,host_screen+220*240,20*240*2)); /* existing footer text unchanged */
    blank(&trk[0]);
    trk[0].step[0].n=1;trk[0].step[0].note[0]=0; /* MIDI note zero is a real note */
    trk[0].step[1].hit=1; /* drum-grid event without pitched note[] */
    trk[0].step[2].time=ST_TIE;
    trk[0].step[3].time=ST_REST;trk[0].step[3].n=1;trk[0].step[3].hit=1; /* stale payload ignored */
    trk[0].step[4].n=1;trk[0].step[4].probability=101; /* stored note, currently silent probability */
    trk[0].step[5].flags=SF_ACCENT; /* flags alone are not a note */
    trk[0].p[P_MUTE]=1;refresh();
    for(unsigned i=0;i<16;i++)REQUIRE(bean(i)==(i==0 || i==1 || i==4));
    trk[0].p[P_MUTE]=0;song.playing=1;trk[0].seq_idx=7;refresh();
    REQUIRE(!bean(7) && pixel(7,4,14)==swap16(T_TEXT));
    REQUIRE(bean(0) && pixel(0,4,14)==swap16(T_BG));
    trk[0].seq_idx=0;refresh();REQUIRE(bean(0) && pixel(0,4,14)==swap16(T_TEXT));
    trk[0].seq_idx=16;refresh();for(unsigned i=0;i<16;i++)REQUIRE(pixel(i,4,14)==swap16(T_BG));
    song.playing=0;song.seq_mode=1;ui.cursor=7;refresh();
    REQUIRE(!bean(7) && pixel(7,4,14)==swap16(home_step_accent(0)) && pixel(7,4,15)==swap16(home_step_accent(0)));
    song.seq_mode=0;
    for(unsigned p=0;p<4;p++){blank(&trk[p]);trk[p].step[p].n=1;trk[p].step[p].note[0]=60+p;}
    song.sel=0;ui.force=1;ui_draw();ui.force=0;
    for(unsigned p=0;p<4;p++) {
        REQUIRE(song.sel==p && scientist_home.selected==p);
        for(unsigned i=0;i<16;i++)REQUIRE(bean(i)==(i==p));
        if(p<3)turn(EN_ALGO,1); /* actual existing channel-selection encoder */
    }
    /* Actual SONG source, not the selected track's local working pattern. */
    chain.running=1;chain.lane[3]=1;memset(chain.source[1].step[3],0,sizeof chain.source[1].step[3]);
    chain.source[1].step[3][9].n=1;chain.source[1].step[3][9].note[0]=72;refresh();
    for(unsigned i=0;i<16;i++)REQUIRE(bean(i)==(i==9));
    chain.running=0;trk[3].p[P_SLEN]=64;ui.bank=3;trk[3].step[63].n=1;trk[3].step[63].note[0]=64;refresh();
    for(unsigned i=0;i<16;i++)REQUIRE(bean(i)==(i==15));
    ui.bank=0;trk[3].p[P_SLEN]=4;refresh();
    for(unsigned i=4;i<16;i++)REQUIRE(pixel(i,4,10)==swap16(T_BG));
    /* Guard invalid display banks without reading beyond the 64-step source. */
    ui.bank=255;refresh();for(unsigned i=0;i<16;i++)REQUIRE(pixel(i,4,1)==swap16(T_BG));
    ui.bank=0;trk[3].p[P_SLEN]=16;static step_t saved[NSTEP];memcpy(saved,trk[3].step,sizeof saved);
    refresh();REQUIRE(!memcmp(saved,trk[3].step,sizeof saved));
    for(unsigned palette=0;palette<NPALETTES;palette++)if(ux_luma(UI_PALETTES[palette].bg)>128) {
        palette_set(palette);ui.force=1;refresh();
        REQUIRE(ux.light && pixel(3,4,1)==swap16(T_TEXT)); /* contrasting outline on light themes */
        REQUIRE(pixel(3,2,6)==swap16(home_step_accent(3)));
        REQUIRE(pixel(3,5,6)==swap16(T_TEXT));break;
    }
    REQUIRE(bar_x(0)>=0 && bar_x(15)+9<=240 && Y_FOOT+16<=240);
    printf("HOME step icons: %u checks passed; stored events, ties/rests, drums, channel/SONG/bank sources, playhead, bounds and unchanged surrounding pixels\n",checks);
    return 0;
}
