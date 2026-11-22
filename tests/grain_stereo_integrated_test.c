/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_GRAIN_STEREO_PROTOTYPE 1
#define FELUCCA_GRAIN_STEREO_INTEGRATION 1
#define FELUCCA_GRAIN_STEREO_ACCOUNTING 1
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static uint64_t checks;static gr_part_t start;static track_t initial;
static int32_t pcm[2048][CTL];static uint64_t history[2048];static unsigned decode[2048];
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"integrated %u: %s\n",__LINE__,#x);return 1;}}while(0)
static uint64_t legacy(track_t*t){uint64_t h=1;uint8_t*p=(uint8_t*)gr_of(t);size_t n=(uint8_t*)gr_of(t)->stereo_side-p;
 for(size_t i=0;i<n;i++)h=h*33+p[i];for(unsigned i=0;i<8;i++)h=h*33+(uint32_t)t->v[0].s[i];return h*33+(uint32_t)gr_rng[0];}
static int shadow(unsigned length,unsigned source){
 if(length){usr_nz[0]=1;usr_zone[0][0]=*gr_zone(13,0);usr_zone[0][0].n=length;source=5;}
 track_t*t=&trk[0];host_preset(t,ENGI_GRAIN,0);eng_mem_claim(t);t->p[P_SPRD]=t->p[P_PAN]=0;t->p[P_VOICE]=V_POLY;t->p[P_E0]=source;t->p[P_E1]=64;t->p[P_E2]=80;t->p[P_E3]=127;t->p[P_E4]=0;t->p[P_E5]=40;t->p[P_E6]=127;t->p[P_E7]=64;t->p[P_GTYPE]=0;
 t->v[0].env=0;t->v[0].note=60;grain_note_on(t,&t->v[0]);t->v[0].active=1;t->v[0].env=1<<24;
 gr_reset(gr_of(t),source,gr_stamp(source));for(unsigned i=0;i<GR_NIDX;i++)gr_build(gr_of(t),t);initial=*t;start=*gr_of(t);
 unsigned settled=0,different=0,sides=0;
 for(unsigned mode=0;mode<2;mode++){
  *t=initial;*gr_of(t)=start;gr_rng[0]=0x12345678;
  for(unsigned b=0;b<2048;b++){
   gr_part_t*P=gr_of(t);gr_test_spread[0]=mode&&b>=16&&b<256?100:0;
   if(mode&&b>=64&&b<128)gr_test_spread[0]=(b%16<8)?0:100;
   gr_stereo_clear(P);P->lr_block=0;int32_t out[CTL]={0};vmod_t m={0};m.pitch16=gr_zone(source,0)->root16-384;m.shape=64<<8;m.amp0=m.amp1=32767;
   t->p[P_GCUT]=(int[]){0,-1,-50,1,50,0,25,-25}[b/32%8];t->p[P_GRES]=b%128;
   unsigned had_placed=0;for(unsigned g=0;g<GR_NG;g++)had_placed|=P->g[g].owner==1&&P->pan[g]!=0;
   unsigned oldmix=P->lr[0].mix,oldon=P->lr[0].on;
   gr_test_decode_calls=0;grain_render(t,&t->v[0],out,CTL,&m);
   if(!mode){memcpy(pcm[b],out,sizeof out);history[b]=legacy(t);decode[b]=gr_test_decode_calls;}
   else{
    if(oldon&&!gr_test_spread[0]){
     CHECK(P->lr[0].mix==(had_placed?oldmix:(oldmix>CTL?oldmix-CTL:0)));
     if(had_placed)CHECK(P->lr[0].on);
    }
    CHECK(history[b]==legacy(t));CHECK(decode[b]==gr_test_decode_calls);CHECK(!gr_test_invalid_reads);
    different+=memcmp(pcm[b],out,sizeof out)!=0;
    for(unsigned i=0;i<CTL;i++){sides+=P->stereo_side[1][i]!=0;CHECK(llabs((int64_t)out[i])<3000000);}
    if(b>256&&!P->lr_block&&!P->lr[0].on){CHECK(!memcmp(pcm[b],out,sizeof out));settled++;for(unsigned i=0;i<CTL;i++)CHECK(!P->stereo_side[0][i]&&!P->stereo_side[1][i]);}
   }
  }
 }
 CHECK(different>50&&sides>100&&settled>1000);printf("local shadow: changed blocks=%u, side frames=%u, exact settled blocks=%u; all RNG/decoder/legacy history checks exact\n",different,sides,settled);
 return 0;
}
static int filter(void){
 for(int cut=-50;cut<=50;cut++)for(unsigned res=0;res<=127;res++){
  gr_lr_filter_t s={0};voice_t ref[3]={{0}};s.tone_a=GR_TONE_UNITY;
  for(unsigned ch=0;ch<3;ch++)ref[ch].s[6]=GR_TONE_UNITY;
  for(unsigned b=0;b<12;b++){
   int32_t input[3][CTL],original[3][CTL],out[3][CTL]={{0}},expected[3][CTL]={{0}};
   int target=gr_tone_coef((int[]){127,0,64}[b/4]);vmod_t m={0};m.amp0=b%2?32767:0;m.amp1=32767;
   for(unsigned ch=0;ch<3;ch++)for(unsigned i=0;i<CTL;i++)input[ch][i]=((b*CTL+i+ch*13)%64<32)?60000:-60000;
   memcpy(original,input,sizeof input);gr_lr_filter(&s,input,out,CTL,&m,target,cut,res,3);
   for(unsigned ch=0;ch<3;ch++){
    gr_tone(&ref[ch],original[ch],CTL,target);gr_filter(&ref[ch],original[ch],expected[ch],CTL,&m,cut,res);
    CHECK(!memcmp(out[ch],expected[ch],sizeof out[ch]));CHECK(s.tone_a==ref[ch].s[6]&&s.filter==ref[ch].s[7]);
    CHECK(s.lane[ch].tone_y==ref[ch].s[5]&&s.lane[ch].last==ref[ch].s[2]&&s.lane[ch].ic1==ref[ch].s[3]&&s.lane[ch].ic2==ref[ch].s[4]);
   }
  }
 }
 return 0;
}
static int lifecycle(void){
 track_t*t=&trk[0];host_preset(t,ENGI_GRAIN,0);eng_mem_claim(t);gr_part_t*P=gr_of(t);
 P->lr[0].on=1;P->lr[0].lane[0].ic1=123;memset(P->stereo_side,0x55,sizeof P->stereo_side);
 t->v[0].env=0;t->v[0].note=60;grain_note_on(t,&t->v[0]);CHECK(!P->lr[0].on&&!P->lr[0].lane[0].ic1);
 gr_test_spread[0]=100;track_defaults(t);CHECK(gr_test_spread[0]==0);
 for(unsigned i=0;i<NVOICE;i++)t->v[i].active=0;gr_stereo_clear(P);grain_block(t);
 for(unsigned i=0;i<GR_POLY;i++)CHECK(!P->lr[i].on);
 for(unsigned i=0;i<2*CTL;i++)CHECK(!((int32_t*)P->stereo_side)[i]);
 uint32_t old=t->engine;t->engine=ENGI_PHYS;CHECK(!track_grain_stereo(t,CTL).group[0]);eng_mem_claim(t);
 t->engine=old;eng_mem_claim(t);CHECK(!P->lr[0].on);for(unsigned i=0;i<2*CTL;i++)CHECK(!((int32_t*)P->stereo_side)[i]);
 return 0;
}
static int component_gates(void){
 track_t*t=&trk[0];t->p[P_SLDEPTH]=127;
 for(unsigned mode=0;mode<3;mode++){
  t->p[P_SLCR]=mode;memset(sl,0,sizeof sl);memset(sl_buf,0,sizeof sl_buf);
  for(unsigned b=0;b<1024;b++){
   sl_t saved=sl[0];static int16_t buffer[SL_LEN];memcpy(buffer,sl_buf[0],sizeof buffer);
   int32_t mono[CTL],d[CTL],a[CTL],c[CTL],ref[CTL],rd[CTL];
   for(unsigned i=0;i<CTL;i++)mono[i]=ref[i]=a[i]=c[i]=d[i]=rd[i]=(i&1)?12000:-12000;
   slicer_track_sd(t,ref,rd,CTL);sl_t final=sl[0];static int16_t result[SL_LEN];memcpy(result,sl_buf[0],sizeof result);
   sl[0]=saved;memcpy(sl_buf[0],buffer,sizeof buffer);slicer_track_grain(t,mono,d,a,c,CTL);
   CHECK(!memcmp(mono,ref,sizeof mono)&&!memcmp(d,rd,sizeof d));CHECK(!memcmp(a,rd,sizeof a)&&!memcmp(c,rd,sizeof c));
   CHECK(!memcmp(&final,&sl[0],sizeof final)&&!memcmp(result,sl_buf[0],sizeof result));
  }
 }
 for(unsigned on=0;on<2;on++){
  pf.act=on?(1u<<PF_M1):0;pf.mg[0]=on?32768:0;
  for(unsigned b=0;b<16;b++){
   int32_t mono[CTL],d[CTL],a[CTL],c[CTL],ref[CTL],rd[CTL];for(unsigned i=0;i<CTL;i++)mono[i]=ref[i]=a[i]=c[i]=d[i]=rd[i]=12000;
   int32_t old=pf.mg[0];perf_mute_sd(0,ref,rd,CTL);int32_t target=pf.mg[0];pf.mg[0]=old;perf_mute_grain(0,mono,d,a,c,CTL);
   CHECK(pf.mg[0]==target&&!memcmp(mono,ref,sizeof mono)&&!memcmp(d,rd,sizeof d)&&!memcmp(a,rd,sizeof a)&&!memcmp(c,rd,sizeof c));
  }
 }
 return 0;
}
static int mixer_equation(void){
 track_t*t=&trk[0];host_preset(t,ENGI_GRAIN,0);eng_mem_claim(t);
 t->p[P_DIST]=t->p[P_ITYPE]=t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=t->p[P_SLCR]=0;t->ins_run=0;pf.mute=0;
 for(unsigned pan_case=0;pan_case<3;pan_case++)for(unsigned spread=0;spread<3;spread++)for(unsigned level=0;level<4;level++){
  int pan=(int[]){-64,0,63}[pan_case],width=(int[]){0,64,127}[spread];
  t->p[P_PAN]=pan;t->p[P_SPRD]=width;t->p[P_LEVEL]=(int[]){0,1,100,127}[level];T_SPREAD(t)=width!=0;
  int s=width?(width+1)>>1:0,pa=clamp(pan-s,-64,63),pb=clamp(pan+s,-64,63);
  int gla=4096-(pa>0?pa*64:0),glb=4096-(pb>0?pb*64:0),gra=4096+(pa<0?pa*64:0),grb=4096+(pb<0?pb*64:0);
  memset(mix_l,0,sizeof mix_l);memset(mix_r,0,sizeof mix_r);memset(sl,0,sizeof sl);
  int32_t mid[CTL];for(unsigned i=0;i<CTL;i++){mid[i]=2000;SPREAD_BUF(t)[i]=4000;gr_of(t)->stereo_side[0][i]=2000;gr_of(t)->stereo_side[1][i]=-1000;}
  mix_grain_stereo(t,mid,CTL SM_ARG(&mx[0]));
  int64_t l=((int64_t)6000*gla-3000*glb)*LEVEL_Q12[t->p[P_LEVEL]],r=((int64_t)2000*gra-1000*grb)*LEVEL_Q12[t->p[P_LEVEL]];
  for(unsigned i=0;i<CTL;i++){CHECK(mix_l[i]==(l>>24));CHECK(mix_r[i]==(r>>24));}
 }
 return 0;
}
static int reset_trace(void){
 uint64_t hash[2]={0};
 for(unsigned pass=0;pass<2;pass++){
  if(pass){gr_test_spread[0]=100;gr_of(&trk[0])->lr[0].on=1;gr_of(&trk[0])->lr[0].mix=256;gr_of(&trk[0])->lr[0].lane[0].ic1=12345;}
  ui_power_on();for(unsigned ch=0;ch<4;ch++)CHECK(!gr_test_spread[ch]);
  track_t*t=&trk[0];host_preset(t,ENGI_GRAIN,0);eng_mem_claim(t);t->p[P_E0]=13;t->p[P_PAN]=t->p[P_SPRD]=0;t->p[P_VOICE]=V_POLY;
  gr_reset(gr_of(t),13,gr_stamp(13));for(unsigned i=0;i<GR_NIDX;i++)gr_build(gr_of(t),t);gr_rng[0]=0x12345678;
  trk_note_on(t,60,100);CHECK(!gr_of(t)->lr[0].on);
  for(unsigned b=0;b<256;b++){
   int32_t out[CTL];mod_begin(t);track_render(t,out,CTL);if(MOD(t).on)mod_end(t);
   CHECK(!gr_of(t)->lr_block&&!gr_of(t)->lr[0].on);
   for(unsigned i=0;i<CTL;i++){hash[pass]=hash[pass]*33+(uint32_t)out[i];CHECK(!gr_of(t)->stereo_side[0][i]&&!gr_of(t)->stereo_side[1][i]);}
  }
 }
 CHECK(hash[0]!=0&&hash[0]==hash[1]);return 0;
}
static int renderer_reference(void){
 ui_power_on();track_t*t=&trk[0];host_preset(t,ENGI_GRAIN,0);eng_mem_claim(t);
 t->p[P_E0]=13;t->p[P_E1]=64;t->p[P_E2]=80;t->p[P_E3]=127;t->p[P_E4]=0;t->p[P_E5]=40;t->p[P_E6]=127;t->p[P_E7]=64;t->p[P_GTYPE]=0;t->p[P_PAN]=t->p[P_SPRD]=0;t->p[P_VOICE]=V_POLY;
 gr_part_t*P=gr_of(t);gr_reset(P,13,gr_stamp(13));for(unsigned i=0;i<GR_NIDX;i++)gr_build(P,t);
 t->v[0].env=0;t->v[0].note=60;grain_note_on(t,&t->v[0]);t->v[0].active=1;t->v[0].env=1<<24;gr_rng[0]=0x12345678;gr_test_spread[0]=100;
 unsigned differences=0;
 for(unsigned b=0;b<512;b++){
  static gr_part_t copy;copy=*P;voice_t ref[2]={t->v[0],t->v[0]};
  if(P->lr[0].on)for(unsigned ch=0;ch<2;ch++){ref[ch].s[2]=P->lr[0].lane[ch].last;ref[ch].s[3]=P->lr[0].lane[ch].ic1;ref[ch].s[4]=P->lr[0].lane[ch].ic2;ref[ch].s[5]=P->lr[0].lane[ch].tone_y;}
  int32_t signal[2][CTL]={{0}},expected[2][CTL]={{0}},out[CTL]={0};
  for(unsigned g=0;g<GR_NG;g++)if(copy.g[g].owner==1){
   int32_t sample[CTL]={0};int pan=copy.pan[g];gr_run(&copy,&copy.g[g],sample,CTL);
   for(unsigned i=0;i<CTL;i++){int64_t x=sample[i];signal[0][i]+=(int32_t)(x-(pan>0?x*pan/64:0));signal[1][i]+=(int32_t)(x-(pan<0?x*(-pan)/64:0));}
  }
  vmod_t m={0};m.pitch16=gr_zone(13,0)->root16-384;m.shape=64<<8;m.amp0=b&1?0:32767;m.amp1=32767;
  t->p[P_GCUT]=(int[]){0,-1,-50,1,50,0,25,-25}[b/32%8];t->p[P_GRES]=b%128;
  for(unsigned ch=0;ch<2;ch++){gr_tone(&ref[ch],signal[ch],CTL,gr_tone_coef(64));gr_filter(&ref[ch],signal[ch],expected[ch],CTL,&m,t->p[P_GCUT],t->p[P_GRES]);}
  gr_stereo_clear(P);P->lr_block=0;grain_render(t,&t->v[0],out,CTL,&m);
  for(unsigned ch=0;ch<2;ch++)CHECK(P->lr[0].lane[ch].last==ref[ch].s[2]&&P->lr[0].lane[ch].ic1==ref[ch].s[3]&&P->lr[0].lane[ch].ic2==ref[ch].s[4]&&P->lr[0].lane[ch].tone_y==ref[ch].s[5]);
  for(unsigned i=0;i<CTL;i++){CHECK((int64_t)out[i]+P->stereo_side[1][i]==expected[0][i]);CHECK((int64_t)out[i]-P->stereo_side[1][i]==expected[1][i]);differences+=expected[0][i]!=expected[1][i];}
 }
 CHECK(differences>100);return 0;
}
int main(void){ui_power_on();CHECK(sizeof(gr_part_t)==8472&&sizeof(gr_lr_voice_t)==36&&sizeof(gr_grain_t)==64&&sizeof eng_mem==51552);CHECK(!filter());CHECK(!component_gates());CHECK(!lifecycle());CHECK(!mixer_equation());for(unsigned source=0;source<5;source++)CHECK(!shadow(0,(unsigned[]){0,2,3,13,14}[source]));
 for(unsigned z=0;z<3;z++)CHECK(!shadow((unsigned[]){32,257,65535}[z],13));CHECK(!reset_trace());CHECK(!renderer_reference());printf("integrated strict: %llu checks passed\n",(unsigned long long)checks);return 0;}
