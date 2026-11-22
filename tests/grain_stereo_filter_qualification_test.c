/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "grain_stereo_filter_qualification.h"
#include <time.h>
static uint64_t checks,frames_tested,clamps_seen,hash;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"LR qualification %u: %s\n",__LINE__,#x);return 1;}}while(0)
static int compare(q_lr_state_t *s,voice_t ref[2],unsigned n,int tone,int cut,int res,unsigned tick){
 struct{int32_t pre,out[2][CTL],post;}out={.pre=12345,.post=67890};
 int32_t input[2][CTL],expected[2][CTL];vmod_t m={0};m.amp0=(tick&1)?32767:0;m.amp1=32767;
 for(unsigned i=0;i<CTL;i++)for(unsigned ch=0;ch<2;ch++){
  unsigned frame=tick*CTL+i;
  input[ch][i]=ch?(((frame+13)%128<64)?30000:-30000):((frame%64<32)?60000:-60000);
  if(tick%17==0)input[ch][i]=ch?INT32_MIN:INT32_MAX;
  expected[ch][i]=out.out[ch][i]=(tick%19==0)?(ch?INT32_MIN+5:INT32_MAX-5):0;
 }
 q_lr_block(s,input,out.out,n,&m,gr_tone_coef(tone),cut,res);
 for(unsigned ch=0;ch<2;ch++){
  int32_t dry[CTL];memcpy(dry,input[ch],sizeof dry);gr_tone(&ref[ch],dry,n,gr_tone_coef(tone));gr_filter(&ref[ch],dry,expected[ch],n,&m,cut,res);
  for(unsigned i=0;i<CTL;i++){CHECK(out.out[ch][i]==expected[ch][i]);hash=hash*33+(uint32_t)out.out[ch][i];}
  CHECK(s->tone_a==ref[ch].s[6]&&s->filter==ref[ch].s[7]);
  CHECK(s->lane[ch].tone_y==ref[ch].s[5]&&s->lane[ch].last==ref[ch].s[2]);
  CHECK(s->lane[ch].ic1==ref[ch].s[3]&&s->lane[ch].ic2==ref[ch].s[4]);
  CHECK(abs(s->lane[ch].ic1)<=150000&&abs(s->lane[ch].ic2)<=150000);
  clamps_seen+=abs(s->lane[ch].ic1)==150000||abs(s->lane[ch].ic2)==150000;
 }
 CHECK(out.pre==12345&&out.post==67890);frames_tested+=n;
 return 0;
}
static int encode(void){
 const int32_t values[]={INT32_MIN,INT32_MIN+1,-65535,-1,0,1,65535,INT32_MAX-1,INT32_MAX};
 for(unsigned l=0;l<9;l++)for(unsigned r=0;r<9;r++){
  int64_t mid=(int64_t)values[l]+values[r],side=(int64_t)values[l]-values[r];
  CHECK((mid+side)/2==values[l]&&(mid-side)/2==values[r]);CHECK(!((mid+side)%2)&&!((mid-side)%2));
 }
 return 0;
}
static int timeline(void){
 q_lr_state_t s={0};s.tone_a=GR_TONE_UNITY;int32_t in[2][CTL]={{0}},out[2][CTL]={{0}};vmod_t m={0};m.amp0=m.amp1=32767;
 for(unsigned i=0;i<256;i++){q_lr_block(&s,in,out,1,&m,GR_TONE_UNITY,1,127);CHECK((s.filter&511)==i+1);}
 for(unsigned i=0;i<256;i++){q_lr_block(&s,in,out,1,&m,GR_TONE_UNITY,-1,127);CHECK((s.filter&511)==255-i);}
 for(unsigned ch=0;ch<2;ch++)CHECK(!s.lane[ch].ic1&&!s.lane[ch].ic2&&!s.lane[ch].last);
 q_lr_block(&s,in,out,1,&m,GR_TONE_UNITY,-1,127);CHECK((s.filter&511)==1);
 int32_t before=s.tone_a,target=gr_tone_coef(0);q_lr_block(&s,in,out,1,&m,target,-1,127);
 CHECK(s.tone_a==before-((before-target)>>8)); /* once, not twice for L/R */
 /* Rejected lengths are a no-op and cannot touch caller buffers/state. */
 q_lr_state_t copy=s;int32_t saved[2][CTL];memcpy(saved,out,sizeof out);
 q_lr_block(&s,in,out,CTL+1,&m,target,0,0);q_lr_block(&s,in,out,0,&m,target,0,0);
 CHECK(!memcmp(&s,&copy,sizeof s)&&!memcmp(saved,out,sizeof out));
 return 0;
}
static int handoff(void){
 const int32_t end[]={INT32_MIN,-65535,-1,0,1,65535,INT32_MAX};
 for(unsigned a=0;a<7;a++)for(unsigned b=0;b<7;b++){
  int32_t previous=end[a];
  for(unsigned w=0;w<=256;w++){
   int64_t delta=(int64_t)end[b]-end[a];int64_t x=(int64_t)end[a]+delta*w/256;
   CHECK(x>=INT32_MIN&&x<=INT32_MAX);CHECK(w!=0||x==end[a]);CHECK(w!=256||x==end[b]);
   if(w){CHECK(llabs(x-previous)<=(llabs(delta)+255)/256);CHECK(delta<0?x<=previous:x>=previous);}previous=(int32_t)x;
  }
 }
 /* Clone warm mono history into explicitly separate lanes, not aliased views. */
 voice_t mono={0};mono.s[6]=GR_TONE_UNITY;vmod_t m={0};m.amp0=m.amp1=32767;
 for(unsigned b=0;b<32;b++){int32_t dry[CTL],out[CTL]={0};for(unsigned i=0;i<CTL;i++)dry[i]=(i&1)?30000:-30000;gr_tone(&mono,dry,CTL,gr_tone_coef(64));gr_filter(&mono,dry,out,CTL,&m,-25,127);}
 q_lr_state_t s={0};s.tone_a=mono.s[6];s.filter=mono.s[7];voice_t ref[2]={mono,mono};
 for(unsigned ch=0;ch<2;ch++){s.lane[ch].tone_y=mono.s[5];s.lane[ch].last=mono.s[2];s.lane[ch].ic1=mono.s[3];s.lane[ch].ic2=mono.s[4];}
 CHECK((void*)&s.lane[0]!=(void*)&s.lane[1]);
 for(unsigned b=0;b<256;b++)CHECK(!compare(&s,ref,CTL,64,(int[]){-25,0,25,0}[b/64],127,b));
 return 0;
}
static int bounds(void){
 int64_t max_product=0,max_signal=65535;
 for(int cutoff=-50;cutoff<=50;cutoff++)for(int res=0;res<=127;res++){
  if(!cutoff)continue;
  int cut=cutoff<0?(46<<8)+((50+cutoff)*(54<<8)/50):(40<<8)+(cutoff*(60<<8)/50);
  tsvf_t fc;tsvf_coef(&fc,cut,res);CHECK(fc.a1>=0&&fc.a2>=0&&fc.a3>=0);
  int64_t a1=fc.a1,a2=fc.a2,a3=fc.a3,k=8192-res*7600/127;
  int64_t p1=a1*150000+a2*215535,p2=a2*150000+a3*215535;
  CHECK(p1<=INT32_MAX&&p2<=INT32_MAX);
  if(p1>max_product)max_product=p1;if(p2>max_product)max_product=p2;
  int64_t lp=150000+(p2+8191)/8192;
  int64_t sample=cutoff<0?lp:65535+(k*150000+4095)/4096+lp;
  if(sample>max_signal)max_signal=sample;
 }
 int64_t voice=((max_signal*32767>>15)*VOICE_FS>>15)*2+4;
 CHECK(2*GR_POLY*voice<INT32_MAX);CHECK(2*8*voice<INT32_MAX);
 printf("Bound proof: max product=%lld, signal=%lld, zero-origin voice=%lld, part M2/S2 <=%lld\n",(long long)max_product,(long long)max_signal,(long long)voice,(long long)(2*GR_POLY*voice));return 0;
}
int main(int argc,char**argv){
 ui_power_on();CHECK(sizeof(q_lr_state_t)==40);CHECK(!encode());CHECK(!timeline());CHECK(!handoff());CHECK(!bounds());
 for(int cut=-50;cut<=50;cut++)for(int res=0;res<=127;res++)for(unsigned tone=0;tone<3;tone++){
  q_lr_state_t s={0};voice_t ref[2]={{0}};s.tone_a=GR_TONE_UNITY;ref[0].s[6]=ref[1].s[6]=GR_TONE_UNITY;
  /* Settled running filter exercises resonance saturation, not only the fade. */
  s.filter=cut?(int32_t)(256u|((uint32_t)(cut+50)<<9)):0;ref[0].s[7]=ref[1].s[7]=s.filter;
  for(unsigned b=0;b<16;b++)CHECK(!compare(&s,ref,CTL,(int[]){0,64,127}[tone],cut,res,b));
 }
 for(unsigned init=0;init<4;init++){
  q_lr_state_t s={0};voice_t ref[2]={{0}};s.filter=(int[]){0,-1,511,INT32_MAX}[init];ref[0].s[7]=ref[1].s[7]=s.filter;
  for(unsigned b=0;b<2048;b++)CHECK(!compare(&s,ref,1+b%CTL,(int[]){127,0,64,126}[b/37%4],(int[]){0,-1,-50,1,50,0,25,-25}[b/13%8],b%128,b));
 }
 /* Exact settled bypass including its original amplitude roundings. */
 {q_lr_state_t s={0};s.tone_a=GR_TONE_UNITY;vmod_t m={0};m.amp0=123;m.amp1=32767;int32_t in[2][CTL],out[2][CTL]={{0}};
  for(unsigned i=0;i<CTL;i++){in[0][i]=i*2000-30000;in[1][i]=-in[0][i];}
  q_lr_block(&s,in,out,CTL,&m,GR_TONE_UNITY,0,127);
  for(unsigned i=0;i<CTL;i++)for(unsigned ch=0;ch<2;ch++)CHECK(out[ch][i]==gr_output(0,in[ch][i],&m,i));CHECK(!s.filter);
 }
 CHECK(clamps_seen>0);printf("LR qualification: %llu checks; %llu frames; %llu clamped lane endpoints; max PCM/state mismatch=0; wide re-encode error=0; hash=%llu\n",(unsigned long long)checks,(unsigned long long)frames_tested,(unsigned long long)clamps_seen,(unsigned long long)hash);
 return 0;
}
