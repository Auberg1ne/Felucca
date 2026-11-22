/* SPDX-License-Identifier: GPL-3.0-only */
/* Frozen Step 237 host oracle. Hash explicit LE fields, never pointers/padding. */
#define UI_TEST_NO_MAIN 1
#ifndef GRANULAR_QUALITY_UI_SOURCE
#define GRANULAR_QUALITY_UI_SOURCE "ui_test.c"
#endif
#include GRANULAR_QUALITY_UI_SOURCE
#include "audio_stats.h"
static uint64_t state=UINT64_C(14695981039346656037);
static void field(uint32_t x){for(unsigned j=0;j<4;j++){state^=(x>>(j*8))&255;state*=UINT64_C(1099511628211);}}
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"quality line %u: %s\n",__LINE__,#x);return 1;}}while(0)
static int mapping(void){
 const unsigned row[4]={103,104,105,255};
 const int defaults[4][2]={{64,64},{96,8},{64,64},{80,64}};
 const char *names[4][2]={{"DRIFT","WIDTH"},{"HOLD","MOTION"},{"RATE","GATE"},{"INTERVAL","HARMONICS"}};
 REQUIRE(P_COUNT==119 && P_GRC==118 && P_E0==110 && P_GWIN==109);
 for(unsigned type=0;type<4;type++){
  TSEL->p[P_GTYPE]=type;
  for(unsigned c=0;c<4;c++)REQUIRE(grain_grid_param(1,0,c)==row[c]);
  for(unsigned c=0;c<2;c++){const param_desc_t*d=track_desc(TSEL,104+c);REQUIRE(!strcmp(d->label,names[type][c]) && d->min==0 && d->max==127 && d->def==defaults[type][c]);}
 }
 return 0;
}
static unsigned short_silent;
static int short_windows(void){
 for(unsigned len=16;len<=128;len++)for(unsigned offset=0;offset<CTL;offset++)for(unsigned rev=0;rev<2;rev++){
  gr_part_t p={0};gr_grain_t*g=&p.g[0];g->z=&SMP_ZONES[0];g->owner=1;g->window=192;g->rev=rev;g->a=g->b=16000;g->gain=32767;g->left=len;g->winc=UINT32_MAX/len;g->pos=1;
  unsigned nonzero=0,done=0;while(g->owner){struct{int32_t pre,v[CTL],post;}out={.pre=12345,.post=67890};unsigned n=done?CTL:CTL-offset;
   /* Preserve Step 237's historical short-envelope hash; the production
    * correction has a separate exhaustive non-silence/state gate. */
#ifdef GRANULAR_QUALITY_LEGACY_SHORT_PROBE
   gr_run_block(&p,g,out.v,n);
#else
   gr_run(&p,g,out.v,n);
#endif
   REQUIRE(out.pre==12345&&out.post==67890);for(unsigned j=0;j<n;j++){field(out.v[j]);nonzero+=out.v[j]!=0;}done++;}
  short_silent+=!nonzero;
 }
 return 0;
}
int main(int argc,char**argv){
 if(argc!=4)return 2;
 unsigned type=atoi(argv[1]),scenario=atoi(argv[2]);if(type>3||scenario>5)return 2;
 ui_power_on();REQUIRE(!mapping());REQUIRE(!short_windows());song.master_q12=4096;
 unsigned tracks=scenario==5?4:1,source[]={0,2,14,13},draws[4]={0},admitted=0,reverse=0,maxload=0,maxactive=0;
 for(unsigned p=0;p<4;p++){
  track_t*t=&trk[p];host_preset(t,ENGI_GRAIN,0);
  unsigned kind=scenario==5?(type+p)%4:type,src=getenv("GRAIN_SOURCE")?atoi(getenv("GRAIN_SOURCE")):source[kind];
  t->p[P_MUTE]=p>=tracks;t->p[P_LEVEL]=100;t->p[P_ATK]=t->p[P_DEC]=t->p[P_REL]=0;t->p[P_SUS]=127;
  t->p[P_DIST]=t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=t->p[P_ITYPE]=0;
  t->p[P_ED_FLT]=t->p[P_LD_FLT]=t->p[P_ED_SHP]=t->p[P_LD_SHP]=t->p[P_LD_AMP]=0;t->p[P_PAN]=t->p[P_SPRD]=0;
  const int e[]={src,64,scenario==2?0:80,scenario==0?30:127,0,40,scenario==3?127:30,127};
  for(unsigned i=0;i<8;i++)t->p[P_E0+i]=e[i];
  t->p[P_GRC]=getenv("GRC_VALUE")?atoi(getenv("GRC_VALUE")):0;REQUIRE(t->p[P_GRC]>=0 && t->p[P_GRC]<=100);
  t->p[P_GTYPE]=kind;t->p[P_GA]=t->p[P_GB]=64;t->p[P_GWIN]=64;t->p[P_GCUT]=0;t->p[P_GRES]=127;t->p[108]=0;
  if(scenario==4){t->p[P_LRATE]=95;t->p[P_LD_FLT]=60;t->p[P_LD_SHP]=50;}
  gr_part_t*g=gr_of(t);memset(g,0,sizeof *g);gr_reset(g,src,gr_stamp(src));for(unsigned i=0;i<GR_NIDX;i++)gr_build(g,t);
  gr_rng[gr_part(t)]=0x12345678u+p*0x10203u;
  if(p<tracks)trk_note_on(t,60+2*p,100);
 }
 FILE*f=fopen(argv[3],"wb");REQUIRE(f);audio_stats_t stats={0};unsigned crosses=0;int lastcut[4]={0};
 unsigned blocks=getenv("GRAIN_BLOCKS")?atoi(getenv("GRAIN_BLOCKS")):2048;REQUIRE(blocks>=2048&&blocks<=65536);
 for(unsigned b=0;b<blocks;b++){
  uint32_t before[4];for(unsigned p=0;p<tracks;p++)before[p]=gr_rng[gr_part(&trk[p])];
  if(scenario==4||scenario==5)for(unsigned p=0;p<tracks;p++)trk[p].p[P_GCUT]=(int[]){0,-1,-25,-50,0,1,25,50}[b/32%8];
  if(getenv("GRC_LIVE"))for(unsigned p=0;p<tracks;p++)trk[p].p[P_GRC]=!strcmp(getenv("GRC_LIVE"),"chaos")?(int[]){0,100,0,25,50,100,0,50}[b/128%8]:(int[]){0,100,25,50}[b/128%4];
  int32_t out[CTL*2];mix_block(out,CTL);
  for(unsigned i=0;i<CTL*2;i++){audio_stats_push(&stats,out[i]);uint32_t x=(uint32_t)out[i];for(unsigned j=0;j<4;j++)REQUIRE(fputc((x>>(8*j))&255,f)!=EOF);}
  for(unsigned p=0;p<tracks;p++){
   track_t*t=&trk[p];gr_part_t*g=gr_of(t);uint32_t r=before[p];unsigned count=0;
   while(r!=gr_rng[gr_part(t)]&&count<256){noise32(&r);count++;}REQUIRE(count<256);draws[p]+=count;field(count);field(r);
   unsigned load=0,active=0;
   for(unsigned v=0;v<GR_POLY;v++){field(t->v[v].s[1]);field(g->head[v].sample);field(g->head[v].frac);field(g->head[v].target);field(g->head_zl[v]);field(t->v[v].s[7]);
    int cut=t->v[v].s[7]? (int)((uint32_t)t->v[v].s[7]>>9)-50:0;if(v==0){crosses+=cut!=lastcut[p];lastcut[p]=cut;}}
   for(unsigned j=0;j<GR_NG;j++){
    gr_grain_t*a=&g->g[j];field(a->owner);if(!a->owner)continue;
    active++;load+=a->step<<a->rev;if(!a->wph){admitted++;reverse+=a->rev;}
    field(a->pos);field(a->frac);field(a->step);field(a->wph);field(a->winc);field(a->left);field(a->a);field(a->b);field(a->pred);field(a->idx);field(a->rlo);field(a->gain);field(a->rev);field(a->zl);field(a->window);
    if(a->rev)for(unsigned k=0;k<GR_RB;k++)field((uint16_t)g->rb[j][k]);
   }
   REQUIRE(load<=GR_LOAD_MAX && active<=GR_NG);if(load>maxload)maxload=load;if(active>maxactive)maxactive=active;field(load);
  }
 }
#ifdef GR_DRIFT_PHASE_STEP
 if(getenv("DRIFT_REPORT"))fprintf(stderr,"DRIFT %u %d\n",gr_of(&trk[0])->drift[0].phase,gr_of(&trk[0])->drift[0].displacement);
#endif
 REQUIRE(!fclose(f));REQUIRE(audio_stats_sounding(&stats,2,0.1));REQUIRE(admitted>0);if(scenario==3)REQUIRE(reverse>0);if(scenario==4)REQUIRE(crosses>10);
 printf("{\"short_silent\":%u,\"state_fnv64\":\"%016llx\",\"rng_draws\":[%u,%u,%u,%u],\"admitted\":%u,\"reverse_admitted\":%u,\"max_load_q16\":%u,\"max_active\":%u,\"filter_changes\":%u,\"peak\":%.0f,\"rms\":%.6f,\"nonzero\":%llu}\n",short_silent,(unsigned long long)state,draws[0],draws[1],draws[2],draws[3],admitted,reverse,maxload,maxactive,crosses,stats.peak,audio_stats_rms(&stats),(unsigned long long)stats.nonzero);
 return 0;
}
