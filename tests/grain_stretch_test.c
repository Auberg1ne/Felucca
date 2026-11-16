/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"stretch %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
 const unsigned values[]={0,1,64,127};
 REQUIRE(P_GFILTER_RSVD1==108 && P_E0==110 && P_COUNT==118 && sizeof(project_store_t)==3840);
 ui_power_on();
 for(unsigned e=0;e<NENGINES;e++)for(unsigned id=0;id<P_COUNT;id++) {
  const param_desc_t *d=param_desc_of(e,id);
  if(e==ENGI_GRAIN && id==108)REQUIRE(d->min==0 && d->max==127 && d->def==0 && d->fmt==F_INT && str_eq(d->label,"STRETCH"));
  else if(e==ENGI_GRAIN && id==109)REQUIRE(d->min==0 && d->max==127 && d->def==64 && str_eq(d->label,"WIN"));
  else REQUIRE(d==(id>=P_E0?&ENGINES[e]->edit[id-P_E0]:&TP[id]));
 }
 for(unsigned n=0;n<NELEM(values);n++) {
  unsigned value=values[n];ui_power_on();
  REQUIRE(TSEL->eng_req==ENGI_GRAIN && track_desc(TSEL,108)==param_desc_of(ENGI_GRAIN,108));
  TSEL->p[108]=value;
  static project_t p,q;static project_store_t stored;
  project_capture(&p);REQUIRE(p.t[0].p[108]==value && proj_pack(&stored,&p));
  REQUIRE(proj_import(&q,&stored,sizeof stored) && q.t[0].p[108]==value);
  TSEL->p[108]=0;REQUIRE(!project_restore_runtime(&q) && TSEL->p[108]==value);
  REQUIRE(!project_save(0));TSEL->p[108]=0;project_load(0);REQUIRE(TSEL->p[108]==value);
  unsigned rc=up_store(0,"STRETCH");REQUIRE(rc==0 || rc==3);REQUIRE(up_used(0) && up_value(up_rec(0),108)==value);
  TSEL->p[108]=0;REQUIRE(!up_load(0) && TSEL->p[108]==value);
  midi_cc_set(TSEL,108,value);REQUIRE(TSEL->p[108]==value);
  REQUIRE(ml_learn(70,0,108)==1);TSEL->p[108]=0;REQUIRE(midi_learned(70,value) && TSEL->p[108]==value);
  REQUIRE(!motion_put(TSEL,0,108,value,0));TSEL->p[108]=0;motion_step(TSEL,0,&motion,1);REQUIRE(TSEL->p[108]==value);
  REQUIRE(!motion_put(TSEL,1,108,value,MOTION_LOCK));
  REQUIRE(motion_put(TSEL,2,108,128,0)==1 && motion_put(TSEL,2,108,-1,0)==1);
  motion_clear(TSEL);ui_home_view=HV_SCOPE;go_home();TSEL->p[108]=0;
  int16_t *vp;REQUIRE(home_param(2,&vp)==param_desc_of(ENGI_GRAIN,108) && vp==&TSEL->p[108]);
  turn(EN_K3,(int)value);REQUIRE(TSEL->p[108]==value); /* no acceleration with default prefs */
 }
 for(unsigned e=0;e<NENGINES;e++)if(e!=ENGI_GRAIN) {
  ui_power_on();set_engine_of(TSEL,e);TSEL->engine=e;TSEL->p[108]=0;
  REQUIRE(track_desc(TSEL,108)->max==0 && param_desc_of(e,108)==&TP[108]);
  midi_cc_set(TSEL,108,127);REQUIRE(TSEL->p[108]==0 && motion_put(TSEL,0,108,1,0)==1);
  static project_t p;project_capture(&p);p.t[0].p[108]=127;p.sum=proj_sum(&p);
  REQUIRE(!project_restore_runtime(&p) && TSEL->p[108]==0);
 }
 printf("GRAIN STRETCH: %u checks passed; IDs/layout and unrelated descriptors unchanged\n",checks);return 0;
}
