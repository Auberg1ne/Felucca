/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <time.h>
static uint64_t checks,gate_settings;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"stutter gate %u: %s\n",__LINE__,#x);return 1;}}while(0)
static void hard(int32_t*a,unsigned n,uint32_t start,unsigned period,unsigned on){for(unsigned i=0;i<n;i++)if((start+i)%period>=on)a[i]=0;}
static int sweep(void){
 const int32_t amplitudes[]={0,1,-1,2,-2,32767,-32767,393216,-393216,INT32_MAX,INT32_MIN};
 for(unsigned rate=0;rate<128;rate++)for(unsigned gate=0;gate<128;gate++){
  unsigned period=128+(127-rate)*12,on=period*gate/127,fade=(on+1)/2;if(fade>period-on)fade=period-on;if(fade>64)fade=64;if(fade<2)fade=2;
  unsigned step=(65536+fade-1)/fade,nonzero=0;uint64_t sum=0,centroid=0;int previous=0,first=0,maxjump=0;
  for(unsigned pos=0;pos<period;){
   struct{uint32_t pre;int32_t a[CTL];uint32_t post;}block={.pre=0x12345678,.post=0xabcdef01};
   unsigned n=1+(pos+rate+gate)%CTL;if(n>period-pos)n=period-pos;for(unsigned j=0;j<CTL;j++)block.a[j]=65536;
   gr_stutter_gate(block.a,n,pos,period,on);CHECK(block.pre==0x12345678&&block.post==0xabcdef01);
   for(unsigned j=n;j<CTL;j++)CHECK(block.a[j]==65536);
   for(unsigned j=0;j<n;j++){
    unsigned phase=pos+j,q=block.a[j];CHECK(q<=65536);CHECK((q!=0)==(phase<on));
    if(gate==127)CHECK(q==65536);if(!gate)CHECK(q==0);
    int jump=phase?abs((int)q-previous):0;if(jump>maxjump)maxjump=jump;if(!phase)first=q;previous=q;
    nonzero+=q!=0;sum+=q;centroid+=(uint64_t)q*phase;
    if(phase<on){unsigned mirror=on-1-phase;int32_t other=65536;gr_stutter_gate(&other,1,mirror,period,on);CHECK(other==(int32_t)q);}
   }
   pos+=n;
  }
  if(abs(previous-first)>maxjump)maxjump=abs(previous-first);
  CHECK(nonzero==on);if(on)CHECK(centroid*2==(on-1)*sum);
  if(on&&on<period)CHECK((unsigned)maxjump<=step&&maxjump<=32768);else CHECK(maxjump==0);
  const unsigned phases[]={0,1,on/2,on?on-1:0,on,period-1};
  for(unsigned a=0;a<NELEM(amplitudes);a++)for(unsigned p=0;p<NELEM(phases);p++){
   int32_t x=amplitudes[a],q=65536;gr_stutter_gate(&q,1,phases[p],period,on);int32_t y=x;gr_stutter_gate(&y,1,phases[p],period,on);
   int64_t expected=(llabs((long long)x)*q+32768)/65536;if(x<0)expected=-expected;
   CHECK(y==expected);if(on&&phases[p]==on/2&&x)CHECK(y!=0);CHECK(llabs((long long)y)<=llabs((long long)x));
   if(x!=INT32_MIN){int32_t neg=-x;gr_stutter_gate(&neg,1,phases[p],period,on);CHECK(neg==-y);} /* signed symmetry: no additive DC */
  }
  gate_settings++;
 }
 return 0;
}
static int fragments(void){
 const uint32_t starts[]={0,1,31,32,63,127,128,1651,UINT32_MAX-32,UINT32_MAX};
 for(unsigned rate=0;rate<128;rate+=127)for(unsigned gate=0;gate<128;gate++)for(unsigned s=0;s<NELEM(starts);s++)for(unsigned split=1;split<=CTL;split++){
  unsigned period=128+(127-rate)*12,on=period*gate/127;int32_t a[CTL],b[CTL];for(unsigned i=0;i<CTL;i++)a[i]=b[i]=(i&1)?-32767:32767;
  gr_stutter_gate(a,CTL,starts[s],period,on);for(unsigned i=0;i<CTL;){unsigned n=CTL-i;if(n>split)n=split;gr_stutter_gate(b+i,n,starts[s]+i,period,on);i+=n;}
  CHECK(!memcmp(a,b,sizeof a));
 }
 return 0;
}
static int render_phase(void){
 ui_power_on();track_t*t=&trk[0];host_preset(t,ENGI_GRAIN,0);t->p[P_E0]=3;t->p[P_E7]=127;t->p[P_GCUT]=0;
 gr_part_t*p=gr_of(t);gr_reset(p,3,gr_stamp(3));for(unsigned i=0;i<GR_NIDX;i++)gr_build(p,t);gr_part_t indexed=*p;
 for(unsigned rate=0;rate<128;rate++)for(unsigned gate=0;gate<128;gate++)for(unsigned trial=0;trial<3;trial++){
  unsigned period=128+(127-rate)*12;uint32_t start=(uint32_t[]){0,period-1,UINT32_MAX-31}[trial];
  *p=indexed;voice_t*v=&t->v[0];memset(v,0,sizeof *v);v->active=1;v->ph[1]=start;v->s[1]=100000;v->s[7]=-1;
  t->p[P_GTYPE]=2;t->p[P_GA]=rate;t->p[P_GB]=gate;
  gr_grain_t*g=&p->g[0];g->z=gr_zone(3,0);g->owner=1;g->left=64;g->a=g->b=16000;g->step=1;g->wph=128u<<24;g->gain=32767;
  vmod_t m={0};m.pitch16=960;m.shape=64<<8;m.amp0=m.amp1=32767;int32_t out[CTL]={0};uint32_t rng=gr_rng[gr_part(t)];
  grain_render(t,v,out,CTL,&m);CHECK(v->ph[1]==start+CTL);CHECK(gr_rng[gr_part(t)]==rng);CHECK(g->left==32&&g->frac==32&&v->s[1]==100000-CTL);
 }
 return 0;
}
static int metrics(void){
 const unsigned rates[]={0,127},gates[]={0,1,64,126,127};
 puts("rate,gate,period,on,fade,old_jump,new_jump");
 for(unsigned r=0;r<NELEM(rates);r++)for(unsigned g=0;g<NELEM(gates);g++){
  unsigned period=128+(127-rates[r])*12,on=period*gates[g]/127,fade=(on+1)/2;if(fade>period-on)fade=period-on;if(fade>64)fade=64;if(fade<2)fade=2;
  int oldprevious=0,newprevious=0,oldjump=0,newjump=0;
  for(unsigned pos=0;pos<2*period;pos++){int32_t a=16384,b=a;hard(&a,1,pos,period,on);gr_stutter_gate(&b,1,pos,period,on);
   if(pos){int d=abs(a-oldprevious);if(d>oldjump)oldjump=d;d=abs(b-newprevious);if(d>newjump)newjump=d;}oldprevious=a;newprevious=b;}
  printf("%u,%u,%u,%u,%u,%d,%d\n",rates[r],gates[g],period,on,(on&&on<period)?fade:0,oldjump,newjump);
 }
 return 0;
}
static volatile int64_t observed;
static uint64_t ns(void){struct timespec t;clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t);return(uint64_t)t.tv_sec*1000000000+t.tv_nsec;}
static int bench(void){
 puts("rate,gate,method,repeat,ns_per_sample");
 for(unsigned rate=0;rate<128;rate+=127)for(unsigned gate=0;gate<128;gate+=1)if(gate==0||gate==1||gate==64||gate==126||gate==127)
 for(unsigned rep=0;rep<5;rep++)for(unsigned method=0;method<2;method++){
  unsigned period=128+(127-rate)*12,on=period*gate/127;void(*run)(int32_t*,unsigned,uint32_t,unsigned,unsigned)=method?gr_stutter_gate:hard;
  uint64_t start=ns();for(unsigned block=0;block<32768;block++){int32_t a[CTL];for(unsigned i=0;i<CTL;i++)a[i]=16000;run(a,CTL,block*CTL,period,on);for(unsigned i=0;i<CTL;i++)observed+=a[i];}
  printf("%u,%u,%u,%u,%.6f\n",rate,gate,method,rep,(double)(ns()-start)/(32768*CTL));
 }
 return observed==INT64_MIN;
}
int main(int argc,char**argv){
 if(argc==2&&!strcmp(argv[1],"--bench"))return bench();
 CHECK(sizeof(gr_grain_t)==64&&sizeof(gr_part_t)==7576);CHECK(!sweep());CHECK(!fragments());CHECK(!render_phase());metrics();
 printf("STUTTER gate: %llu checks; %llu rate/gate settings; unity/support/centroid/signed symmetry/block and uint32 wrap passed\n",(unsigned long long)checks,(unsigned long long)gate_settings);return 0;
}
