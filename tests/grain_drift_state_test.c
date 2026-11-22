/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"DRIFT state %u: %s\n",__LINE__,#x);return 1;}}while(0)
static void setup(track_t*t,unsigned src){host_preset(t,ENGI_GRAIN,0);t->p[P_GTYPE]=1;t->p[P_GRC]=100;t->p[P_E0]=src;t->p[P_GCUT]=0;eng_mem_claim(t);gr_part_t*P=gr_of(t);gr_reset(P,src,gr_stamp(src));for(unsigned k=0;k<GR_NIDX;k++)gr_build(P,t);memset(t->v,0,sizeof t->v);t->v[0].active=1;t->v[0].note=60;grain_note_on(t,&t->v[0]);}
int main(void){
 ui_power_on();CHECK(sizeof(gr_drift_t)==8);CHECK(sizeof(gr_part_t)==7320&&sizeof(gr_grain_t)==64&&sizeof(track_t)==1840);CHECK(sizeof eng_mem==NPART*ENG_MEM_PART && ENG_MEM_PART==12888);
 CHECK((uint8_t*)&gr_of(&trk[0])->drift[0]-(uint8_t*)gr_of(&trk[0])==7296);
 struct{uint32_t pre;gr_drift_t d;uint32_t post;}g={12345,{0,98765},67890};uint64_t frames=0;
 for(unsigned n=1;n<=32;n++)for(unsigned k=0;k<65536;k++){gr_drift_tick(&g.d,n);frames+=n;CHECK(g.d.phase==(uint32_t)(frames*GR_DRIFT_PHASE_STEP));CHECK(g.d.displacement==98765&&g.pre==12345&&g.post==67890);}
 gr_drift_reset(&g.d);CHECK(!g.d.phase&&!g.d.displacement);g.d.phase=UINT32_MAX-100;gr_drift_tick(&g.d,32);CHECK(g.d.phase==(uint32_t)(UINT32_MAX-100u+32u*GR_DRIFT_PHASE_STEP));
 for(unsigned ch=0;ch<NTRK;ch++){
  track_t*t=&trk[ch];setup(t,3);gr_part_t*P=gr_of(t);voice_t*v=&t->v[0];CHECK(!P->drift[0].phase&&!P->drift[0].displacement);
  P->drift[0]=(gr_drift_t){123456,0};v->env=123;v->s[0]=0;grain_note_on(t,v);CHECK(P->drift[0].phase==123456); /* sounding same-zone recapture */
  v->s[0]=-1;grain_note_on(t,v);CHECK(!P->drift[0].phase);P->drift[0]=(gr_drift_t){789,456};v->env=0;grain_note_on(t,v);CHECK(!P->drift[0].phase&&!P->drift[0].displacement);
  v->active=1;vmod_t m={0};m.pitch16=960;m.shape=64<<8;m.amp0=m.amp1=32767;
  uint32_t expected=0;for(unsigned b=0;b<4096;b++){
   unsigned c=(unsigned[]){0,100,0,25,50,100,0,50}[b/128%8];t->p[P_GRC]=c;grain_block(t);
   uint32_t ph[3];memcpy(ph,v->ph,sizeof ph);struct{int32_t pre,out[CTL],post;}out={.pre=12345,.post=67890};grain_render(t,v,out.out,CTL,&m);if(c)expected+=CTL*GR_DRIFT_PHASE_STEP;
   CHECK(P->drift[0].phase==expected&&abs(P->drift[0].displacement)<=GR_DRIFT_LIMIT);CHECK(out.pre==12345&&out.post==67890);CHECK(!memcmp(v->ph,ph,sizeof ph));
   song.sel=(ch+b)%NTRK;CHECK(P==gr_of(t));
  }
  /* Rendering one voice cannot move another voice's clock. */
  for(unsigned i=0;i<GR_POLY;i++){t->v[i].active=1;t->v[i].s[0]=0;P->drift[i]=(gr_drift_t){100+i,0};}
  t->p[P_GRC]=100;grain_block(t);int32_t one[CTL]={0};grain_render(t,&t->v[1],one,7,&m);
  CHECK(P->drift[0].phase==100&&P->drift[1].phase==101+7*GR_DRIFT_PHASE_STEP&&P->drift[2].phase==102);
  t->v[1].active=t->v[2].active=0;grain_block(t);CHECK(!P->drift[1].phase&&!P->drift[2].phase);
  /* Selecting tracks/stopping without a note event cannot reset the oscillator. */
  unsigned old=P->drift[0].phase;song.playing=0;grain_block(t);CHECK(P->drift[0].phase==old);seq_start();CHECK(P->drift[0].phase==old);seq_stop();CHECK(P->drift[0].phase==old);
  /* Inactive voice, fallback, personality and source/upload reset boundaries. */
  v->active=0;grain_block(t);CHECK(!P->drift[0].phase);v->active=1;P->drift[0]=(gr_drift_t){7,9};v->s[0]=GR_SINE;grain_block(t);CHECK(!P->drift[0].phase&&!P->drift[0].displacement);
  v->s[0]=0;P->drift[0]=(gr_drift_t){7,9};t->p[P_GTYPE]=2;grain_block(t);CHECK(!P->drift[0].phase&&!P->drift[0].displacement);t->p[P_GTYPE]=1;
  P->drift[0]=(gr_drift_t){7,9};t->p[P_E0]=13;grain_block(t);CHECK(!P->drift[0].phase&&!P->drift[0].displacement);
  P->drift[0]=(gr_drift_t){7,9};P->stamp^=1;grain_block(t);CHECK(!P->drift[0].phase&&!P->drift[0].displacement);
  /* Displacement may change source starts, never rate/head/gain/RNG or ownership. */
  gr_reset(P,3,gr_stamp(3));t->p[P_E0]=3;for(unsigned k=0;k<GR_NIDX;k++)gr_build(P,t);
  static gr_part_t first,second;*P=(gr_part_t){0};gr_reset(P,3,gr_stamp(3));for(unsigned k=0;k<GR_NIDX;k++)gr_build(P,t);first=*P;second=first;second.drift[0]=(gr_drift_t){UINT32_MAX,GR_DRIFT_LIMIT};
  gr_rng[ch]=12345;gr_spawn_one(&first,t,0,0,1000,&m,0,32767);unsigned r=gr_rng[ch];gr_rng[ch]=12345;gr_spawn_one(&second,t,0,0,1000,&m,0,32767);CHECK(gr_rng[ch]==r);CHECK(!memcmp(first.head,second.head,sizeof first.head));CHECK(first.g[0].step==second.g[0].step&&first.g[0].left==second.g[0].left&&first.g[0].gain==second.g[0].gain&&first.g[0].rev==second.g[0].rev);
 }
 /* Real user-zone reassignment and common voice-start lifecycle. */
 track_t*t=&trk[0];setup(t,3);gr_part_t*P=gr_of(t);voice_t*v=&t->v[0];usr_nz[0]=2;
 usr_zone[0][0]=usr_zone[0][1]=*gr_zone(3,0);usr_zone[0][0].lo=0;usr_zone[0][0].hi=60;usr_zone[0][1].lo=61;usr_zone[0][1].hi=127;
 t->p[P_E0]=5;grain_block(t);v->s[0]=0;v->env=123;v->active=1;v->stage=2;P->drift[0]=(gr_drift_t){999,0};
 voice_start(t,v,60,100,0);CHECK(P->drift[0].phase==999&&v->s[0]==0);
 voice_start(t,v,72,100,0);CHECK(!P->drift[0].phase&&v->s[0]==1);
 /* Ownership: every part is isolated; a different memory-owning engine claims
  * and clears its own region, then GRAIN clears its expanded extent on return. */
 static uint8_t copy[sizeof eng_mem];memset(eng_mem,0x5a,sizeof eng_mem);memcpy(copy,eng_mem,sizeof copy);memset(eng_mem_owner,0,sizeof eng_mem_owner);
 for(unsigned ch=0;ch<NPART;ch++){
  trk[ch].engine=ENGI_GRAIN;eng_mem_claim(&trk[ch]);CHECK(eng_mem_owner[ch]==ENGI_GRAIN+1);
  for(unsigned i=0;i<ENG_MEM_PART;i++)CHECK(eng_mem[ch].b[i]==(i<sizeof(gr_part_t)?0:0x5a));
  for(unsigned other=ch+1;other<NPART;other++)CHECK(!memcmp(eng_mem[other].b,copy+other*sizeof eng_mem[0],ENG_MEM_PART));
  gr_of(&trk[ch])->drift[0].phase=42;eng_mem_claim(&trk[ch]);CHECK(gr_of(&trk[ch])->drift[0].phase==42);
#if !FELUCCA_GRAIN_ONLY
  trk[ch].engine=ENGI_PHYS;eng_mem_claim(&trk[ch]);CHECK(eng_mem_owner[ch]==ENGI_PHYS+1);for(unsigned i=0;i<ENGINES[ENGI_PHYS]->mem;i++)CHECK(eng_mem[ch].b[i]==0);
  memset(eng_mem[ch].b,0x7b,ENGINES[ENGI_PHYS]->mem);trk[ch].engine=ENGI_GRAIN;eng_mem_claim(&trk[ch]);for(unsigned i=0;i<sizeof(gr_part_t);i++)CHECK(eng_mem[ch].b[i]==0);
#endif
 }
 printf("DRIFT foundation: %u checks; clock wrap, lifecycle, bounded displacement and reservation/engine ownership passed\n",checks);return 0;
}
