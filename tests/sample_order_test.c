/* SPDX-License-Identifier: GPL-3.0-only */
/* Display positions must never become serialized sample source IDs. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define REQUIRE(x) do { checks++; if (!(x)) { fprintf(stderr,"sample order line %u: %s\n",__LINE__,#x);return 1; } } while (0)
int main(void)
{
    static const uint8_t order[] = {0,2,3,13,14,5,6,7,8,9,10,11,12};
    static const char *const names[] = {"CELLO","FLUTE","GUITAR","SYNTH","VOX","USR1","USR2","USR3","USR4","USR5","USR6","USR7","USR8"};
    static const uint8_t engines[] = {ENGI_SAMPLE, ENGI_GRAIN};
    for (unsigned e=0;e<NELEM(engines);e++) {
        ui_power_on(); set_engine_of(TSEL,engines[e]);
        const param_desc_t *d=&ENGINES[engines[e]]->edit[0];
        REQUIRE(d->min==0 && d->max==14 && d->names==SMP_ALL_NAMES);
        for (unsigned v=0;v<15;v++) {
            REQUIRE(param_fit(d,v)==(int32_t)v); /* raw project/editor IDs remain unchanged */
            REQUIRE(param_turn(d,v,0)==(int32_t)v);
            REQUIRE(enum_orig(d,v)==(int32_t)((v==1 || v==4)?0:v));
        }
        REQUIRE(param_turn(d,1,1)==2 && param_turn(d,4,1)==2);
        REQUIRE(param_turn(d,1,-1)==0 && param_turn(d,4,-1)==0);
        REQUIRE(enum_rank(d,1)==0 && enum_rank(d,4)==0);
        for (unsigned i=0;i<NELEM(order);i++) {
            REQUIRE(!strcmp(d->names[order[i]],names[i]));
            REQUIRE(enum_rank(d,order[i])==(int32_t)(i*14u/12u));
            REQUIRE(param_turn(d,order[i],1)==order[i+1<NELEM(order)?i+1:i]);
            REQUIRE(param_turn(d,order[i],-1)==order[i?i-1:0]);
            REQUIRE(param_turn(d,order[i],100)==12 && param_turn(d,order[i],-100)==0);
            for (unsigned j=0;j<NELEM(order);j++)
                REQUIRE(param_turn(d,order[i],(int32_t)j-(int32_t)i)==order[j]);
            TSEL->p[P_E0]=order[i];
            static project_t p,q;static project_store_t packed;
            project_capture(&p);
            REQUIRE(proj_pack(&packed,&p) && proj_import(&q,&packed,sizeof packed));
            REQUIRE(q.t[song.sel].p[P_E0]==order[i] && q.t[song.sel].engine==engines[e]);
            int rc=up_store(i,names[i]);REQUIRE(rc==0 || rc==3);
            TSEL->p[P_E0]=0;REQUIRE(up_load(i)==0 && TSEL->p[P_E0]==order[i]);
        }
        /* Real EDIT encoder: forward/reverse, both clamped endpoints, displayed names. */
        TSEL->p[P_E0]=0;go_title("EDIT 1");frame();
        turn(EN_K1,-1);REQUIRE(TSEL->p[P_E0]==0);
        for (unsigned i=1;i<NELEM(order);i++) {
            turn(EN_K1,1);REQUIRE(TSEL->p[P_E0]==order[i]);
            int16_t *value=0;const param_desc_t *shown=page_desc(cur_page(),0,&value);
            REQUIRE(shown && value==&TSEL->p[P_E0]);
            REQUIRE(!strcmp(shown->names[*value],names[i]));
        }
        turn(EN_K1,1);REQUIRE(TSEL->p[P_E0]==12);
        for (int i=(int)NELEM(order)-2;i>=0;i--) {turn(EN_K1,-1);REQUIRE(TSEL->p[P_E0]==order[i]);}
        /* Alternate UI editors must use the same order and store raw IDs. */
        TSEL->p[P_E0]=3;
        REQUIRE(mom_turn(0,1) && TSEL->p[P_E0]==13);
        mom_restore();REQUIRE(TSEL->p[P_E0]==3);
        motion_clear(TSEL);ui.plk_src=ui.page+1u;
        REQUIRE(lock_id(0)==P_E0);
        lock_turn(0,1,1u);
        int16_t locked=0;REQUIRE(motion_lock_get(TSEL,0,P_E0,&locked) && locked==13);
        lock_turn(0,-1,1u);REQUIRE(motion_lock_get(TSEL,0,P_E0,&locked) && locked==3);
        motion_clear(TSEL);REQUIRE(!motion_set_event(TSEL,0,P_E0,3));
        int event=motion_find(TSEL,0,P_E0);REQUIRE(event>=0);
        go_page(GR_EVENTS);ev_follow(EVC(EVK_REC,event));
        ev_knob(3,1);REQUIRE(motion.event[event].value==13);
        ev_knob(3,-1);REQUIRE(motion.event[event].value==3);
    }
    /* Factory-only preset browsing still wraps, without changing preset/PC numbers. */
    ui_power_on();set_engine_of(TSEL,ENGI_SAMPLE);
    static const uint8_t presets[]={0,2,3,5,6};
    for (unsigned i=0;i<5;i++) {REQUIRE(TSEL->preset==presets[i] && TSEL->p[P_E0]==order[i]);eng_list_step(1);}
    REQUIRE(TSEL->preset==0 && TSEL->p[P_E0]==0);eng_list_step(-1);
    REQUIRE(TSEL->preset==6 && TSEL->p[P_E0]==14);eng_list_step(1);
    REQUIRE(TSEL->preset==0 && TSEL->p[P_E0]==0);
    for (unsigned i=0;i<5;i++) {midi_pc_load(TSEL,i);REQUIRE(TSEL->preset==presets[i] && TSEL->p[P_E0]==order[i]);}
    REQUIRE(SLC_SRC_PIANO==4 && !strcmp(N_SLC_SRC[4],"GUITAR"));
    printf("sample order: %u checks passed (IDs, names, encoders, endpoints, preset wrap, projects/presets)\n",checks);
    return 0;
}
