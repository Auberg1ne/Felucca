/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#ifndef GRAIN_TRANSITION_UI_SOURCE
#define GRAIN_TRANSITION_UI_SOURCE "ui_test.c"
#endif
#include GRAIN_TRANSITION_UI_SOURCE
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"transition line %u: %s\n",__LINE__,#x); return 1; } } while(0)
/* DC is deliberate: any edge here is introduced by switching, not the source. */
static void constant(track_t*t,voice_t*v,int cut,int mod,int res,int32_t*out)
{
 gr_part_t*p=gr_of(t);t->p[P_GCUT]=cut;t->p[P_GRES]=res;v->s[1]=100000;
 for(unsigned j=0;j<GR_NG;j++)memset(&p->g[j],0,sizeof p->g[j]);
 gr_grain_t*g=&p->g[0];g->z=gr_zone(3,0);g->owner=1;g->left=64;g->a=g->b=16000;g->step=1;g->wph=128u<<24;g->winc=0;g->gain=32767;
 vmod_t m={0};m.pitch16=960;m.shape=64<<8;m.amp0=m.amp1=32767;m.cutoff=mod*256;
 for(unsigned i=0;i<CTL;i++)out[i]=0;grain_render(t,v,out,CTL,&m);
}
#ifndef TRANSITION_MEASURE_BASE
/* Independent steady-state filter oracle: unchanged coefficients and integer path. */
static void wet_reference(int cut,int res,int32_t*acc,int32_t*out,unsigned n,const vmod_t*m,int32_t*s1,int32_t*s2){
 int q=cut<0?(46<<8)+(50+cut)*(54<<8)/50:(40<<8)+cut*(60<<8)/50;tsvf_t c;tsvf_coef(&c,q,res);
 for(unsigned i=0;i<n;i++){int32_t old=*s1,lp=tsvf_lp(&c,acc[i],s1,s2),bp=(*s1+old)>>1;int32_t y=cut<0?lp:acc[i]-(((8192-res*7600/127)*bp)>>12)-lp;out[i]=gr_output(out[i],y,m,i);}
}
static int kernel(void){
 for(int cut=-50;cut<=50;cut++)for(int res=0;res<=127;res+=127){
  voice_t v={0};v.s[7]=-1;vmod_t m={0};m.amp0=0;m.amp1=65527;
  int32_t a[CTL],out[CTL+2],ref[CTL+2],s1=0,s2=0;
  for(unsigned b=0;b<20;b++){
   for(unsigned i=0;i<CTL;i++)a[i]=(int32_t)((b*CTL+i)*193%131071)-65535;
   for(unsigned i=0;i<CTL+2;i++)out[i]=ref[i]=(i&1)?INT32_MAX-123:INT32_MIN+123;
   if(cut)wet_reference(cut,res,a,ref+1,CTL,&m,&s1,&s2);
   else for(unsigned i=0;i<CTL;i++)ref[i+1]=gr_output(ref[i+1],a[i],&m,i);
   gr_filter(&v,a,out+1,CTL,&m,cut,res);CHECK(!memcmp(out,ref,sizeof out));CHECK(abs(v.s[3])<=150000&&abs(v.s[4])<=150000);
  }
 }
 /* Exact zero after the bounded fade, even with resonance and retained energy. */
 for(int sign=-1;sign<=1;sign+=2)for(int res=0;res<=127;res+=127){
  voice_t v={0};v.s[7]=-1;vmod_t m={0};m.amp0=32767;m.amp1=65527;
  int32_t a[CTL],out[CTL+2],ref[CTL+2];for(unsigned i=0;i<CTL;i++)a[i]=16000;
  for(unsigned b=0;b<100;b++){memset(out,0,sizeof out);gr_filter(&v,a,out+1,CTL,&m,sign,res);}
  for(unsigned b=0;b<8;b++){memset(out,0,sizeof out);gr_filter(&v,a,out+1,CTL,&m,0,res);}
  CHECK((v.s[7]&511)==0 && v.s[3]==0 && v.s[4]==0);
  for(unsigned b=0;b<20;b++){
   for(unsigned i=0;i<CTL+2;i++)out[i]=ref[i]=456789;
   for(unsigned i=0;i<CTL;i++)ref[i+1]=gr_output(ref[i+1],a[i],&m,i);
   gr_filter(&v,a,out+1,CTL,&m,0,res);CHECK(!memcmp(out,ref,sizeof out));
  }
 }
 /* Rapid reversals, variable render lengths, source/state extremes and canaries. */
 uint32_t rng=12345;voice_t v={0};v.s[7]=-1;
 for(unsigned b=0;b<20000;b++){
  rng=rng*1664525u+1013904223u;int cut=(int)(rng%101)-50,res=(rng>>16)&127;unsigned n=b%33;
  int32_t a[CTL],out[CTL+2];for(unsigned i=0;i<CTL;i++)a[i]=i&1?-65535:65535;
  for(unsigned i=0;i<CTL+2;i++)out[i]=123456789;vmod_t m={0};m.amp0=b&1?65527:0;m.amp1=b&1?0:65527;
  gr_filter(&v,a,out+1,n,&m,cut,res);CHECK(out[0]==123456789&&out[n+1]==123456789);CHECK(abs(v.s[3])<=150000&&abs(v.s[4])<=150000);CHECK((v.s[7]&511)<=256);
 }
 return 0;
}
/* A sign reversal must finish fading the old polarity to dry before the
 * opposite filter starts. Check the bridge with partial and full blocks. */
static int polarity_bridge(void)
{
 const int mags[]={1,25,50};
 for(unsigned mi=0;mi<NELEM(mags);mi++)for(int polarity=-1;polarity<=1;polarity+=2)for(int res=0;res<=127;res+=127)
 for(unsigned n=1;n<=CTL;n++){
  int sign=polarity*mags[mi];
  voice_t v={0};v.s[7]=-1;vmod_t m={0};m.amp0=m.amp1=32767;
  int32_t input[CTL],out[CTL];for(unsigned i=0;i<CTL;i++)input[i]=16000;
  memset(out,0,sizeof out);gr_filter(&v,input,out,n,&m,sign,res);
  unsigned left=256;
  while(left){
   memset(out,0,sizeof out);gr_filter(&v,input,out,n,&m,-sign,res);
   left=left>n?left-n:0;
   CHECK(((unsigned)v.s[7]&511)==left);
   CHECK((int)((unsigned)v.s[7]>>9)-50==sign);
  }
  CHECK(v.s[2]==0 && v.s[3]==0 && v.s[4]==0);
  memset(out,0,sizeof out);gr_filter(&v,input,out,n,&m,-sign,res);
  CHECK(out[0]==gr_output(0,input[0],&m,0));
  CHECK(((unsigned)v.s[7]&511)==n);
  CHECK((int)((unsigned)v.s[7]>>9)-50==-sign);
 }
 return 0;
}

/* Bypass ramps count samples, not blocks. Compare every entry offset and
 * fragment size to a one-sample reference, including interrupted fades. */
static int block_boundaries(void)
{
 const int mags[]={1,25,50};
 for(unsigned mi=0;mi<NELEM(mags);mi++)for(unsigned offset=0;offset<CTL;offset++)for(unsigned fragment=1;fragment<=CTL;fragment++)
 for(int polarity=-1;polarity<=1;polarity+=2)for(int res=0;res<=127;res+=127){
  int sign=polarity*mags[mi];
  voice_t a={0},b={0};vmod_t m={0};m.amp0=m.amp1=32767;
  const unsigned lengths[]={offset+1,97,5,7,256,300};
  const int cuts[]={0,sign,0,sign,0,0};
  for(unsigned phase=0;phase<NELEM(lengths);phase++){
   for(unsigned done=0;done<lengths[phase];){
    unsigned n=lengths[phase]-done;if(n>fragment)n=fragment;
    int32_t input[CTL],x[CTL]={0},y[CTL]={0};
    for(unsigned i=0;i<n;i++)input[i]=((done+i)&1)?-16000:16000;
    gr_filter(&a,input,x,n,&m,cuts[phase],res);
    for(unsigned i=0;i<n;i++)gr_filter(&b,input+i,y+i,1,&m,cuts[phase],res);
    CHECK(!memcmp(x,y,n*sizeof x[0]));
    unsigned wa=(unsigned)a.s[7]&511,wb=(unsigned)b.s[7]&511;
    /* Last-cutoff bits are immaterial once dry; both re-seed at next entry. */
    CHECK(wa==wb && (!wa || a.s[7]==b.s[7]));
    CHECK(a.s[2]==b.s[2] && a.s[3]==b.s[3] && a.s[4]==b.s[4]);
    done+=n;
   }
  }
  CHECK((a.s[7]&511)==0 && a.s[3]==0 && a.s[4]==0);
 }
 return 0;
}

/* Exercise the real per-voice LFO and MOD MATRIX -> cutoff publication paths. */
static int routing(void){
 ui_power_on();
 for(unsigned ti=0;ti<4;ti++)for(unsigned route=0;route<4;route++){
  track_t*t=&trk[ti];host_preset(t,ENGI_GRAIN,0);t->engine=t->eng_req=ENGI_GRAIN;t->xf=t->xf_on=0;
  t->p[P_E0]=5;usr_nz[0]=0;t->p[P_E7]=127;t->p[P_GCUT]=0;t->p[P_LD_FLT]=route==0?2:0;t->p[P_ED_FLT]=0;
  t->p[P_ATK]=t->p[P_DEC]=0;t->p[P_SUS]=127;t->p[P_LPOL]=0;t->p[P_M1SRC]=route==1?MS_LFO:0;t->p[P_M1DST]=MD_CUT;t->p[P_M1AMT]=2;
  voice_t*v=&t->v[0];memset(v,0,sizeof*v);v->active=v->gate=1;v->note=60;v->vel=100;v->stage=2;v->env=1<<24;v->pitch_cur=v->pitch16=960;grain_note_on(t,v);v->s[7]=0;
  if(route==3){motion_clear(t);CHECK(!motion_set_lock(t,1,P_GCUT,1));CHECK(!motion_set_lock(t,3,P_GCUT,-1));motion.on|=1u<<ti;}
  for(unsigned phase=0;phase<5;phase++){
   int target=(int[]){0,1,0,-1,0}[phase];
   if(route==2)t->p[P_ED_FLT]=target*2;
   if(route==3)motion_step(t,phase,&motion,1);
   for(unsigned b=0;b<12;b++){
    t->lfo_val=route<2?target*20000:0;t->lfo_fade=32767;int32_t out[CTL];mod_begin(t);CHECK(track_render(t,out,CTL)==1);mod_end(t);
    unsigned wet=(unsigned)v->s[7]&511;CHECK(wet<=256);if(b>=8){if(!(target?wet==256:wet==0))fprintf(stderr,"route=%u track=%u phase=%u target=%d block=%u wet=%u env=%d\n",route,ti,phase,target,b,wet,v->env);CHECK(target?wet==256:wet==0);}
   }
   CHECK(t->p[P_GCUT]==(route==3?target:0)); /* locks restore; modulation never rewrites cutoff */
  }
 }
 return 0;
}

#endif
int main(void){
 const int targets[]={-50,-25,-1,1,25,50};
 unsigned worst=0;for(unsigned type=0;type<4;type++)for(unsigned mod=0;mod<2;mod++)for(unsigned r=0;r<2;r++)for(unsigned ti=0;ti<NELEM(targets);ti++){
  int target=targets[ti];
  ui_power_on();track_t*t=&trk[0];host_preset(t,ENGI_GRAIN,0);t->p[P_E0]=3;t->p[P_E7]=127;t->p[P_GTYPE]=type;t->p[P_GB]=127;
  gr_part_t*p=gr_of(t);memset(p,0,sizeof*p);gr_reset(p,3,0);voice_t*v=&t->v[0];memset(v,0,sizeof*v);v->note=60;v->active=1;grain_note_on(t,v);v->s[0]=0;
  int32_t out[CTL],prior=0;constant(t,v,0,0,r?127:0,out);prior=out[CTL-1];
  constant(t,v,mod?0:target,mod?target:0,r?127:0,out);unsigned jump=(unsigned)abs(out[0]-prior);if(jump>worst)worst=jump;
#ifndef TRANSITION_MEASURE_BASE
  CHECK(jump<16);
#endif
  unsigned maxstep=jump;
  for(unsigned i=0;i<CTL;i++){unsigned d=(unsigned)abs(out[i]-prior);if(d>maxstep)maxstep=d;prior=out[i];}
  for(unsigned phase=0;phase<6;phase++)for(unsigned block=0;block<32;block++){
   int c=(int[]){target,0,-target,target,-target,0}[phase];
   constant(t,v,mod?0:c,mod?c:0,r?127:0,out);
   for(unsigned i=0;i<CTL;i++){unsigned d=(unsigned)abs(out[i]-prior);if(d>maxstep)maxstep=d;prior=out[i];}
#ifndef TRANSITION_MEASURE_BASE
   if(!c && block>=8){for(unsigned i=1;i<CTL;i++)CHECK(out[i]==out[0]);CHECK(v->s[3]==0&&v->s[4]==0);}
#endif
  }
#ifndef TRANSITION_MEASURE_BASE
  CHECK(maxstep<=1024); /* DC fixture: no full-scale switch edge */
#endif
  printf("type=%u modulation=%u res=%u target=%d first-edge=%u max-step=%u\n",type,mod,r?127:0,target,jump,maxstep);

 }
#ifndef TRANSITION_MEASURE_BASE
 if(kernel() || polarity_bridge() || block_boundaries() || routing())return 1;
#endif
 printf("GRAIN zero transition: %u checks; worst first edge=%u\n",checks,worst);return 0;
}
