/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#ifndef EDIT_UI_SOURCE
#define EDIT_UI_SOURCE "ui_test.c"
#endif
#include EDIT_UI_SOURCE
#include "audio_stats.h"
static unsigned checks,cases,shots;
#define REQUIRE(x) do { checks++;if(!(x)){fprintf(stderr,"long EDIT line %u: %s\n",__LINE__,#x);return 1;} }while(0)
static void shot(FILE *f)
{
    uint32_t s[]={song.sel,ui.home,ui.page,ui.menu,ui.layer,ui.lock,ui.confirm,song.playing,grain_edit_row};
    fwrite(s,sizeof s,1,f);fwrite(host_screen,sizeof host_screen,1,f);
    for(unsigned t=0;t<NTRK;t++){fwrite(trk[t].p,sizeof trk[t].p,1,f);fwrite(trk[t].step,sizeof trk[t].step,1,f);fwrite(&trk[t].preset,1,1,f);fwrite(&trk[t].user,1,1,f);}
    fwrite(song.g,sizeof song.g,1,f);shots++;
}
static void setup(unsigned track,int page,unsigned home,unsigned menu)
{
    ui_power_on();memset(&lys,0,sizeof lys);song.sel=track;ui.home=home;ui.menu=menu;
    if(page>=0){ui.page=(unsigned)page;page_entered();}
    ui.msg_t=0;ui.force=1;frame();ui.msg_t=0;sync_reload=0;panic_req=0;
}
static int inert(unsigned ms,int late,int wrap)
{
    static project_t before,after;project_capture(&before);unsigned page=ui.page,home=ui.home,menu=ui.menu,confirm=ui.confirm;
    uint8_t fav[sizeof favorites],undo_before[sizeof undo];memcpy(fav,&favorites,sizeof fav);memcpy(undo_before,&undo,sizeof undo);
    unsigned pref=ui_prefs,pref2=ui_prefs2,row=grain_edit_row,playing=song.playing,naming=name_on();
    uint8_t bank_before[sizeof up_bank],users[NTRK];memcpy(bank_before,up_bank,sizeof up_bank);for(unsigned t=0;t<NTRK;t++)users[t]=trk[t].user;
    if(wrap)host_ticks=UINT32_MAX-100000u;
    btn_down(B_EDIT);frame();
    if(late){host_ticks+=(ms+80u)*1000u*FM1_TICKS_PER_US;fm1_ms+=ms+80u;}
    else {
        for(unsigned i=0;i<(ms+80u)/16u+1u;i++) {
            frame();REQUIRE(ui.layer==0 && ui.lock==0 && ui.page==page && ui.home==home && ui.menu==menu && ui.confirm==confirm && name_on()==naming);
            REQUIRE(!sync_reload && !panic_req && !ui.msg_t);
        }
        REQUIRE(ui.ly_t0 & LY_DEAD);
        for(unsigned i=0;i<16;i++)frame(); /* no repeated short action on a continued hold */
    }
    btn_up(B_EDIT);frame();
    REQUIRE(!ui.layer && !ui.lock && !ui.ly && !ui.ly_t0);
    REQUIRE(ui.page==page && ui.home==home && ui.menu==menu && ui.confirm==confirm && grain_edit_row==row && name_on()==naming);
    REQUIRE(song.playing==playing && !sync_reload && !panic_req && !ui.msg_t);
    project_capture(&after);REQUIRE(!memcmp(&before,&after,sizeof before));
    REQUIRE(!memcmp(fav,&favorites,sizeof fav) && !memcmp(undo_before,&undo,sizeof undo) && ui_prefs==pref && ui_prefs2==pref2);
    REQUIRE(!memcmp(bank_before,up_bank,sizeof up_bank));for(unsigned t=0;t<NTRK;t++)REQUIRE(users[t]==trk[t].user);
    cases++;return 0;
}
static int holds(void)
{
    for(unsigned track=0;track<NTRK;track++)for(unsigned timing=0;timing<4;timing++)for(unsigned playing=0;playing<2;playing++) {
        for(unsigned page=0;page<NPAGES;page++) {
            setup(track,(int)page,0,0);if(!page_visible(page))continue;settings_hold=timing;song.playing=playing;
            if(inert(HOLD_MS[timing],0,0))return 1;
        }
        for(unsigned h=0;h<3;h++) {
            setup(track,-1,1,0);ui.home=h+1;settings_hold=timing;song.playing=playing;
            if(inert(HOLD_MS[timing],0,0))return 1;
        }
        for(unsigned tab=0;tab<MTAB_COUNT;tab++) {
            setup(track,-1,0,1);ui.menu_sel=menu_visible_row(tab,0);settings_hold=timing;song.playing=playing;
            if(inert(HOLD_MS[timing],0,0))return 1;
        }
        setup(track,-1,1,0);settings_hold=timing;if(inert(HOLD_MS[timing],1,0) || inert(HOLD_MS[timing],0,1))return 1;
        for(unsigned i=0;i<3;i++)if(inert(HOLD_MS[timing],0,0))return 1;
    }
    /* Extra modal views must not acquire a release-time EDIT action. */
    setup(0,-1,1,0);ui.confirm=CF_CLEAR_SEQ;if(inert(HOLD_MS[settings_hold%4],0,0))return 1;
    setup(0,-1,1,0);name_open(NK_PROJ_SAVE,0);if(inert(HOLD_MS[settings_hold%4],0,0))return 1;
    /* A pending double tap must not lock the menu after its second press is held. */
    setup(0,-1,1,0);press(B_EDIT);ui.msg_t=0;sync_reload=panic_req=0;
    if(inert(HOLD_MS[settings_hold%4],0,0))return 1;
    return 0;
}
static int boundaries(void)
{
    for(unsigned t=0;t<NTRK;t++)for(unsigned h=0;h<4;h++)for(int edge=-1;edge<=1;edge++) {
        setup(t,-1,1,0);settings_hold=h;btn_down(B_EDIT);frame();
        uint32_t start=ui.ly_t0&~15u,threshold=(uint32_t)HOLD_MS[h]*1000u*FM1_TICKS_PER_US;
        host_ticks=start+threshold+edge;fm1_ms+=HOLD_MS[h];btn_up(B_EDIT);ui_input();ui_draw();
        REQUIRE(!ui.layer && !ui.lock);
        REQUIRE(edge<0?grain_grid_active():ui.home==1 && !ui.msg_t);
    }
    for(unsigned t=0;t<NTRK;t++)for(unsigned type=0;type<4;type++)for(unsigned page=0;page<2;page++) {
        setup(t,-1,1,0);TSEL->p[P_GTYPE]=type;press(B_EDIT);frames(400);if(page)press(B_EDIT);
        turn(EN_SELECT,2);ui.msg_t=0;sync_reload=panic_req=0;
        REQUIRE(grain_grid_active() && grain_edit_row==2);
        if(inert(HOLD_MS[settings_hold%4],0,0))return 1;
    }
    setup(0,-1,1,0);int stored=up_store(0,"EXISTING");REQUIRE(stored==0 || stored==3);go_page(GR_USER);ui.msg_t=0;sync_reload=panic_req=0;
    if(inert(HOLD_MS[settings_hold%4],0,0))return 1;
    setup(0,-1,1,0);REQUIRE(!project_save(0));go_page(GR_SLOTS);ui.msg_t=0;sync_reload=panic_req=0;
    if(inert(HOLD_MS[settings_hold%4],0,0))return 1;
    for(unsigned t=0;t<NTRK;t++) {
        setup(t,-1,1,0);go_page(GR_ROLL);song.seq_mode=1;ui.cursor=3;
        TSEL->step[3].n=1;TSEL->step[3].note[0]=60;TSEL->step[3].vel=100;TSEL->step[3].time=ST_NOTE;
        ui.msg_t=0;sync_reload=panic_req=0;if(inert(HOLD_MS[settings_hold%4],0,0))return 1;
        REQUIRE(ui.cursor==3 && TSEL->step[3].n==1);
    }
    for(unsigned menu=2;menu<=3;menu++){setup(0,-1,0,menu);if(inert(HOLD_MS[settings_hold%4],0,0))return 1;}
    setup(0,-1,1,0);if(inert(HOLD_MS[settings_hold%4],0,0))return 1;press(B_EDIT);REQUIRE(grain_grid_active() && !ui.layer);
    return 0;
}
static int unchanged(FILE *f)
{
    for(unsigned track=0;track<NTRK;track++)for(unsigned page=0;page<NPAGES;page++) {
        setup(track,(int)page,0,0);if(!page_visible(page))continue;shot(f);press(B_EDIT);shot(f);
    }
    const unsigned other[]={B_HOME,B_FX,B_GLO,B_SCL,B_SEQ,B_REC,B_SAVE};
    for(unsigned track=0;track<NTRK;track++)for(unsigned b=0;b<NELEM(other);b++) {setup(track,-1,1,0);hold(other[b]);shot(f);}
    for(unsigned track=0;track<NTRK;track++) {
        setup(track,-1,1,0);press(B_EDIT);press(B_EDIT);shot(f); /* existing double-tap lock */
        setup(track,-1,1,0);btn_down(B_EDIT);frame();turn(EN_K2,1);shot(f);btn_up(B_EDIT);frame();shot(f); /* explicit combo */
        setup(track,-1,1,0);press(B_EDIT);frames(400);turn(EN_SELECT,1);turn(EN_K1,1);shot(f);press(B_EDIT);shot(f);
    }
    return 0;
}
static int routes(void)
{
    setup(0,-1,1,0);for(unsigned p=0;p<4;p++){midi_pc_load(TSEL,p);REQUIRE(TSEL->preset==p && TSEL->eng_req==ENGI_GRAIN);}
    TSEL->p[P_GTYPE]=3;int rc=up_store(0,"RETAINED");REQUIRE(rc==0 || rc==3);TSEL->p[P_GTYPE]=0;REQUIRE(!up_load(0) && TSEL->p[P_GTYPE]==3);
    REQUIRE(!project_save(1));TSEL->p[P_GTYPE]=0;REQUIRE(!project_load(1) && TSEL->p[P_GTYPE]==3);
    undo.keep=0;int16_t before[P_COUNT];memcpy(before,TSEL->p,sizeof before);apply_preset(1);undo_swap();REQUIRE(!memcmp(before,TSEL->p,sizeof before));
    return 0;
}
static int playback(unsigned held,const char *path)
{
    setup(0,-1,1,0);song.master_q12=4096;song.g[G_BPM]=240;ui_rec_prefs=0;settings_hold=0;
    memset(gr_rng,0,sizeof gr_rng);
    for(unsigned p=0;p<NTRK;p++) {
        track_t *t=&trk[p];t->p[P_ATK]=t->p[P_DEC]=t->p[P_REL]=0;t->p[P_SUS]=127;t->p[P_MUTE]=0;t->p[P_LEVEL]=100;t->p[P_SDIV]=0;
        t->p[P_DIST]=t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=t->p[P_ITYPE]=0;t->p[P_E7]=127;t->p[P_GCUT]=0;
        gr_part_t *g=gr_of(t);memset(g,0,sizeof *g);gr_reset(g,t->p[P_E0],gr_stamp(t->p[P_E0]));for(unsigned j=0;j<GR_NIDX;j++)gr_build(g,t);
        memset(t->step,0,sizeof t->step);for(unsigned j=0;j<16;j++){t->step[j].n=1;t->step[j].note[0]=60+2*p;t->step[j].vel=100;t->step[j].time=ST_NOTE;}
    }
    FILE *f=path?fopen(path,"wb"):NULL;if(path && !f)return 2;
    seq_start();if(held)btn_down(B_EDIT);audio_stats_t stats={0};
    for(unsigned b=0;b<2048;b++) {
        if(b%22==0)frame();events_block(CTL);int32_t out[CTL*2];mix_block(out,CTL);
        REQUIRE(song.playing && !ui.layer);if(f && fwrite(out,sizeof out,1,f)!=1)return 2;
        for(unsigned i=0;i<CTL*2;i++)audio_stats_push(&stats,out[i]);
    }
    REQUIRE(audio_stats_sounding(&stats,10,1) && TSEL->seq_idx>0);
    if(held){btn_up(B_EDIT);frame();REQUIRE(ui.home==1 && !ui.layer);}
    printf("EDIT playback held=%u peak=%.0f RMS=%.6f nonzero=%llu seq-step=%u\n",held,stats.peak,audio_stats_rms(&stats),(unsigned long long)stats.nonzero,TSEL->seq_idx);
    seq_stop();return f?fclose(f)!=0:0;
}
int main(int argc,char **argv)
{
    if(argc==4 && !strcmp(argv[1],"--audio"))return playback(atoi(argv[2])!=0,argv[3]);
    if(argc==2){FILE*f=fopen(argv[1],"wb");if(!f)return 2;unchanged(f);fclose(f);printf("EDIT retained snapshots: %u\n",shots);return 0;}
    if(holds() || boundaries() || routes() || playback(1,NULL))return 1;
    printf("Disabled long EDIT: %u checks passed, %u hold cases; all pages/tracks/timings, release, repeats, wrap and modal contexts\n",checks,cases);return 0;
}
