/* SPDX-License-Identifier: GPL-3.0-only */
/* Factory source ABI, real browsing/rendering, presets and persisted projects. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "old_pack.h"
static unsigned checks;
#define REQUIRE(x) do { checks++; if (!(x)) { fprintf(stderr,"factory line %u: %s\n",__LINE__,#x);exit(1); } } while(0)
static uint32_t audio(uint32_t src)
{
    track_t *t=&trk[0]; host_preset(t,ENGI_SAMPLE,0);t->p[P_E0]=src;
    voice_t v={0};v.note=60;v.active=1;sample_note_on(t,&v);
    REQUIRE(!v.s[6]);
    vmod_t m={0};m.pitch16=60*16;m.amp0=m.amp1=32767;
    uint32_t h=2166136261u;unsigned nonzero=0;
    for(unsigned j=0;j<128;j++) {
        int32_t out[32]={0};sample_render(t,&v,out,32,&m);
        for(unsigned i=0;i<32;i++){nonzero|=out[i]!=0;h=(h^(uint32_t)out[i])*16777619u;}
    }
    REQUIRE(nonzero);return h;
}
int main(void)
{
    ui_power_on();
    const unsigned sources[]={0,2,3,13,14},presets[]={0,2,3,5,6};
    const char *names[]={"GUITAR","VOICE","HIT","FLUTE","SAX"};
    REQUIRE(SMP_NSETS==5 && SMP_NFACTORY==7 && SMP_NALL==15);
    REQUIRE(ENG_SAMPLE.npresets==7 && preset_shown(ENGI_SAMPLE)==5);
    REQUIRE(ENG_SAMPLE.edit[0].max==14 && ENG_GRAIN.edit[0].max==14);
    for(unsigned k=0;k<8;k++) {
        REQUIRE(smp_user_from_source(5+k)==k);
        REQUIRE(smp_factory_from_source(5+k)==SMP_NFACTORY);
        REQUIRE(gr_zone(5+k,0)==&usr_zone[k][0]);
        char name[]="USR1";name[3]+=k;REQUIRE(!strcmp(SMP_ALL_NAMES[5+k],name));
    }
    REQUIRE(smp_user_from_source(13)==8 && smp_user_from_source(14)==8);
    REQUIRE(smp_factory_from_source(UINT32_MAX)==SMP_NFACTORY);
    REQUIRE(smp_user_from_source(UINT32_MAX)==8 && smp_set_missing(UINT32_MAX));
    REQUIRE(smp_source_orig(1)==0 && smp_source_orig(4)==0);
    REQUIRE(preset_orig(&ENG_SAMPLE,1)==0 && preset_orig(&ENG_SAMPLE,4)==0);
    REQUIRE(!strcmp(N_SLC_SRC[4],"GUITAR") && SLC_SRC_PIANO==4);
    REQUIRE(SLC_PIANO.seg[0].off==SMP_ZONES[SMP_SETS[0].z0].off);
    uint32_t hashes[5];
    for(unsigned i=0;i<5;i++) {
        unsigned f=smp_factory_from_source(sources[i]);
        REQUIRE(f<SMP_NFACTORY && !strcmp(SMP_ALL_NAMES[sources[i]],names[i]));
        REQUIRE(!strcmp(SMP_SETS[f].name,names[i]) && !smp_set_missing(sources[i]));
        REQUIRE(gr_nz(sources[i])==SMP_SETS[f].nz && gr_find(sources[i],60)>=0);
        REQUIRE(gr_zone(sources[i],0)==&SMP_ZONES[SMP_SETS[f].z0]);
        REQUIRE(enum_orig(&ENG_SAMPLE.edit[0],sources[i])==(int32_t)sources[i]);
        hashes[i]=audio(sources[i]);
        for(unsigned j=0;j<i;j++) REQUIRE(hashes[i]!=hashes[j]);
        set_engine_of(TSEL,ENGI_SAMPLE);apply_preset_to(TSEL,presets[i]);
        REQUIRE(TSEL->preset==presets[i] && TSEL->p[P_E0]==sources[i]);
        last_note=60;REQUIRE(sample_wave_zone(TSEL)<0x8000u);
        REQUIRE(preset_rank(&ENG_SAMPLE,presets[i])==i);
        REQUIRE(preset_cat(ENGI_SAMPLE,presets[i])!=CAT_OTHER);
        /* Factory favourite indices retain their own stable bits. */
        favorites.factory[ENGI_SAMPLE][0]|=1u<<presets[i];
        REQUIRE(favorite_has(ENGI_SAMPLE,presets[i]));
        static project_t q,r;static project_store_t s;project_capture(&q);
        REQUIRE(proj_pack(&s,&q) && proj_import(&r,&s,sizeof s));
        REQUIRE(r.t[song.sel].p[P_E0]==sources[i] && r.t[song.sel].preset==presets[i]);
        { int rc=up_store(i,names[i]);REQUIRE(rc==0 || rc==3); }TSEL->p[P_E0]=0;REQUIRE(up_load(i)==0);
        REQUIRE(TSEL->p[P_E0]==sources[i]);
        set_engine_of(TSEL,ENGI_GRAIN);TSEL->p[P_E0]=sources[i];
        project_capture(&q);REQUIRE(proj_pack(&s,&q) && proj_import(&r,&s,sizeof s));
        REQUIRE(r.t[song.sel].engine==ENGI_GRAIN && r.t[song.sel].p[P_E0]==sources[i]);
        { int rc=up_store(i+5,names[i]);REQUIRE(rc==0 || rc==3); }
        TSEL->p[P_E0]=0;REQUIRE(up_load(i+5)==0 && TSEL->p[P_E0]==sources[i]);
    }
    REQUIRE(hashes[0]==0x2cda0c61u && hashes[1]==0x276769ebu && hashes[2]==0x4780ccc7u);
    REQUIRE(audio(1)==hashes[0] && audio(4)==hashes[0]);
    memset(up_bank,0,sizeof up_bank);memset(&favorites,0,sizeof favorites);
    set_engine_of(TSEL,ENGI_SAMPLE);
    for(unsigned i=0;i<5;i++) {
        REQUIRE(TSEL->preset==presets[i] && TSEL->p[P_E0]==sources[i]);
        eng_list_step(1);
    }
    REQUIRE(TSEL->preset==0 && TSEL->p[P_E0]==0);
    /* Settings round trip preserves existing and new stars; PERC migrates to DRUM. */
    persist_t settings;settings_export(&settings);
    settings.favorites.factory[ENGI_SAMPLE][0]=(1u<<0)|(1u<<1)|(1u<<2)|(1u<<3)|(1u<<4)|(1u<<5)|(1u<<6);
    REQUIRE(settings_import(&settings,sizeof settings)==1);
    REQUIRE(favorite_has(ENGI_DRUM,0) && !favorite_has(ENGI_SAMPLE,4));
    for(unsigned i=0;i<5;i++) REQUIRE(favorite_has(ENGI_SAMPLE,presets[i]));
    set_engine_of(TSEL,ENGI_SAMPLE);apply_preset_to(TSEL,4);REQUIRE(TSEL->eng_req==ENGI_DRUM);
    set_engine_of(TSEL,ENGI_SAMPLE);apply_preset_to(TSEL,1);REQUIRE(TSEL->preset==0 && TSEL->p[P_E0]==0);
    /* Independent FUN9 records: all old sources and PERC still import identically. */
    for(unsigned src=0;src<=12;src++) {
        static project_t q,r;static uint8_t old[3648];project_capture(&q);
        q.t[0].engine=ENGI_SAMPLE;q.t[0].p[P_E0]=src;q.motion.count=0;q.chain.count=0;
        REQUIRE(old_pack(old,&q,OLD_FUN9,99)==sizeof old);
        REQUIRE(proj_import(&r,old,sizeof old));
        REQUIRE(src==4 ? r.t[0].engine==ENGI_DRUM : r.t[0].engine==ENGI_SAMPLE && r.t[0].p[P_E0]==src);
    }
    printf("factory samples: %u checks passed; audio hashes",checks);
    for(unsigned i=0;i<5;i++)printf(" %08x",hashes[i]);puts("");return 0;
}
