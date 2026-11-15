/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#define COFFEE_HOME_VISUALS 1
#include "ui_test.c"
static unsigned checks;
#define REQUIRE(x) do { checks++;if(!(x)){fprintf(stderr,"ENV grid %u: %s\n",__LINE__,#x);return 1;} }while(0)
static int clean_frame(void)
{
    static uint16_t before[240*240];
    ui.help_t=ui.msg_t=ui.hot_t=ui.bpm_t=0;frame();
    memcpy(before,host_screen,sizeof before);ui.force=1;frame();
    REQUIRE(!memcmp(before+28*240,host_screen+28*240,(Y_FOOT-28)*240*2));return 0;
}
int main(void)
{
    static const unsigned ids[2][4]={{P_ATK,P_DEC,P_SUS,P_REL},{P_ED_FLT,P_ED_PIT,P_ED_SHP,P_ESYNC}};
    ui_power_on();unsigned env=page_first(FAM_ENV),dest=env+1;
    REQUIRE(env_grid_page(&PAGES[env]) && !page_visible(dest));
    REQUIRE(env_grid_param(2,0)==255 && env_grid_param(0,4)==255);
    for(unsigned t=0;t<4;t++)for(unsigned pref=SEL_BPM;pref<=SEL_VEL;pref++) {
        go_home();track_select(t);menu_put(MI_SELECT,pref);unsigned saved=ui_prefs2;
        press(B_ENV);frames(400);REQUIRE(!layer_knobs_quiet());REQUIRE(cur_page()->fam==FAM_ENV && grain_edit_row==0 && parameter_grid_active());
        unsigned visible=0;for(unsigned i=0;i<NPAGES;i++)if(PAGES[i].fam==FAM_ENV && page_visible(i))visible++;
        REQUIRE(visible==1);
        for(unsigned row=0;row<2;row++) {
            REQUIRE(grain_edit_row==row);
            for(unsigned c=0;c<4;c++) {
                unsigned id=ids[row][c];int16_t *p;const param_desc_t*d=page_desc(cur_page(),c,&p);
                REQUIRE(page_param_id(cur_page(),c)==id && p==&TSEL->p[id] && d==track_desc(TSEL,id));
                TSEL->p[id]=d->min;int16_t before[P_COUNT];memcpy(before,TSEL->p,sizeof before);turn(EN_K1+c,1);
                REQUIRE(TSEL->p[id]>before[id]);
                ui.ml.on=1;turn(EN_K1+c,1);REQUIRE(ui.ml.pick && ui.ml.id==id && ui.ml.trk==song.sel);ui.ml.on=0;ui.ml.pick=0;
                before[id]=TSEL->p[id];
                for(unsigned k=0;k<P_COUNT;k++)if(k!=id)REQUIRE(TSEL->p[k]==before[k]);
                if(clean_frame())return 1;
            }
            turn(EN_SELECT,1);
        }
        REQUIRE(grain_edit_row==0 && ui.page==env && ui_prefs2==saved);
        turn(EN_SELECT,-1);REQUIRE(grain_edit_row==1);turn(EN_SELECT,-1);REQUIRE(grain_edit_row==0);
        turn(EN_SELECT,3);REQUIRE(grain_edit_row==1);turn(EN_SELECT,-3);REQUIRE(grain_edit_row==0);
        for(unsigned n=0;n<3;n++){press(B_ENV);REQUIRE(ui.page==env && grain_edit_row==0);}
        turn(EN_SELECT,1);track_select((t+1)%4);REQUIRE(grain_edit_row==0);
        int bpm=song.g[G_BPM];btn_down(B_GLO);frame();turn(EN_SELECT,1);btn_up(B_GLO);frame();
        REQUIRE(song.g[G_BPM]>bpm && grain_edit_row==0 && ui_prefs2==saved);
        go_home();REQUIRE(sel_mode()==pref);
    }
    /* Same real HOME note/rest/tie symbols and independent playhead on every track. */
    static uint16_t foot[240*16];
    for(unsigned ch=0;ch<4;ch++)for(unsigned playing=0;playing<2;playing++) {
        song.sel=ch;song.playing=playing;TSEL->seq_idx=5;TSEL->p[P_SLEN]=16;
        memset(TSEL->step,0,sizeof TSEL->step);TSEL->step[0].n=1;TSEL->step[0].note[0]=60;
        TSEL->step[1].time=ST_TIE;TSEL->step[2].time=ST_REST;
        go_home();ui.help_t=ui.msg_t=0;ui.force=1;draw_foot();memcpy(foot,host_screen+Y_FOOT*240,sizeof foot);
        press(B_ENV);ui.help_t=ui.msg_t=0;ui.force=1;draw_foot();
        REQUIRE(!memcmp(foot,host_screen+Y_FOOT*240,sizeof foot));
    }
    song.playing=0;
    /* Stale remembered/direct ENV DEST state normalizes to ENV, not EDIT. */
    go_home();ui.fam_last[FAM_ENV]=dest;press(B_ENV);REQUIRE(ui.page==env);
    ui.page=dest;ui.home=0;ui.force=1;frame();REQUIRE(ui.page==env && grain_edit_row==0);
    /* The shared row resolver also feeds locks after entering STEP. */
    press(B_ENV);turn(EN_SELECT,1);unsigned src=ui.plk_src;press(B_SEQ);
    REQUIRE(ui.plk_src==src && lock_id(0)==P_ED_FLT && lock_id(3)==255 && !motion_param(P_ESYNC));
    TSEL->p[P_ED_FLT]=0;lock_turn(0,10,1ull<<2);int16_t v;
    REQUIRE(motion_lock_get(TSEL,2,P_ED_FLT,&v) && v==10);
    unsigned count=motion.count;lock_turn(3,1,1ull<<2);REQUIRE(motion.count==count);motion_clear(TSEL);
    go_home();press(B_ENV);turn(EN_SELECT,1);song.rec=(uint8_t)(1u<<song.sel);seq_start();seq_tick(TSEL,CTL);
    TSEL->p[P_ED_PIT]=0;turn(EN_K2,10);REQUIRE(motion.count==1 && motion.event[0].param==P_ED_PIT);
    seq_stop();motion_clear(TSEL);
    /* Momentary modifiers use the same selected row and restore the original. */
    go_home();press(B_ENV);turn(EN_SELECT,1);int old=TSEL->p[P_ED_SHP];
    btn_down(B_LFO);frame();turn(EN_SELECT,1);REQUIRE(grain_edit_row==1);
    turn(EN_K3,1);REQUIRE(TSEL->p[P_ED_SHP]==old+1);btn_up(B_LFO);frame();REQUIRE(TSEL->p[P_ED_SHP]==old);
    /* Real curve: every ADSR value changes its pixels, incremental == fresh redraw. */
    go_home();press(B_ENV);static uint16_t graph[240*44];
    for(unsigned k=0;k<4;k++) {
        TSEL->p[ids[0][k]]=10;frame();memcpy(graph,host_screen+140*240,sizeof graph);
        TSEL->p[ids[0][k]]=110;frame();REQUIRE(memcmp(graph,host_screen+140*240,sizeof graph));
    }
    for(unsigned n=0;n<128;n++) {
        for(unsigned k=0;k<4;k++)TSEL->p[ids[0][k]]=(n*(k+1))%128;
        if(clean_frame())return 1;
    }
    /* Unchanged stored parameter representation and real runtime reload. */
    static project_t p,q;static project_store_t store;project_capture(&p);
    REQUIRE(proj_pack(&store,&p) && proj_import(&q,&store,sizeof store));
    memset(TSEL->p,0,sizeof TSEL->p);REQUIRE(!project_restore_runtime(&q));
    for(unsigned t=0;t<4;t++)for(unsigned r=0;r<2;r++)for(unsigned c=0;c<4;c++)REQUIRE(trk[t].p[ids[r][c]]==p.t[t].p[ids[r][c]]);
    int rc=up_store(0,"ENV GRID");REQUIRE(rc==0 || rc==3);
    TSEL->p[P_ED_FLT]=0;REQUIRE(!up_load(0) && TSEL->p[P_ED_FLT]==p.t[song.sel].p[P_ED_FLT]);
    undo.keep=0;int16_t saved[P_COUNT];memcpy(saved,TSEL->p,sizeof saved);apply_preset(1);undo_swap();REQUIRE(!memcmp(saved,TSEL->p,sizeof saved));
    /* GRAIN's 3-row mapping and page tap remain separate, unchanged. */
    go_home();press(B_EDIT);REQUIRE(grain_edit_row==0 && parameter_grid_rows(cur_page())==3);
    turn(EN_SELECT,2);REQUIRE(grain_edit_row==2 && page_param_id(cur_page(),0)==108);
    frames(400);press(B_EDIT);REQUIRE(grain_grid_page(cur_page())==2 && grain_edit_row==0);
    REQUIRE(!parameter_grid_active() || !env_grid_page(cur_page()));
    printf("ENV grid: %u checks passed\n",checks);return 0;
}
