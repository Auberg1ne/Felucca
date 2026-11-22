/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static uint64_t checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"cloud width %u: %s\n",__LINE__,#x);return 1;}}while(0)
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

int main(void){
 ui_power_on();CHECK(!FELUCCA_GRAIN_STEREO_INTEGRATION&&FELUCCA_GRAIN_CLOUD_WIDTH);CHECK(sizeof(cloud_width_t)==580&&sizeof(gr_part_t)==8160&&sizeof eng_mem==51552);CHECK(ENG_GRAIN.render==grain_render);
 CHECK(!component_gates());
 int32_t rng_before[NPART];memcpy(rng_before,gr_rng,sizeof gr_rng);
 for(unsigned mode=0;mode<2;mode++)for(unsigned c=0;c<=100;c++){
  cloud_width_t s={0};uint32_t seed=0x12345678;
  for(unsigned i=0;i<2048;i++){
   int32_t x=(int16_t)noise32(&seed)*16;int32_t side=cw_step(&s,x,c,mode,16);
   CHECK(s.a<CW_A&&s.b<CW_B&&s.gain>=0&&s.gain<=CW_MAX);
   CHECK((int64_t)(x+side)+(x-side)==2*(int64_t)x);
   CHECK(llabs(side)<=2*524288);for(unsigned j=0;j<CW_A+CW_B;j++)CHECK(llabs(s.ring[j])<=2*524288);
   if(!c)CHECK(!side&&!s.gain);
  }
  for(unsigned i=0;i<256;i++)cw_step(&s,1000,0,mode,16);
  CHECK(!s.gain&&!s.a&&!s.b);for(unsigned j=0;j<CW_A+CW_B;j++)CHECK(!s.ring[j]);
 }
 CHECK(!memcmp(rng_before,gr_rng,sizeof gr_rng));
 for(unsigned pan=0;pan<128;pan++){
  cloud_width_t s={0};s.side[0]=123456;int32_t l=100000,r=-20000,oldl=l,oldr=r;
  cloud_add(&s,0,LEVEL_Q12[127],(int)pan-64,&l,&r);
  CHECK((int64_t)l+r==(int64_t)oldl+oldr);if(!pan)CHECK(l==oldl&&r==oldr);
 }
 track_t*t=&trk[0];host_preset(t,ENGI_GRAIN,0);eng_mem_claim(t);t->p[P_GTYPE]=0;t->p[P_GRC]=100;t->v[0].active=1;int32_t b[CTL];for(unsigned i=0;i<CTL;i++)b[i]=10000;
 cloud_width_t*s=cloud_prepare(t,b,CTL);CHECK(s&&s->gain>0);
 t->v[0].active=0;CHECK(cloud_tail(t));for(unsigned i=0;i<8;i++)cloud_prepare(t,b,CTL);CHECK(!cloud_tail(t)&&!s->gain);t->v[0].active=1;cloud_prepare(t,b,CTL);
 t->p[P_GRC]=0;for(unsigned i=0;i<8;i++)cloud_prepare(t,b,CTL);CHECK(!s->gain);CHECK(!cloud_prepare(t,b,CTL));
 t->p[P_GRC]=100;cloud_prepare(t,b,CTL);track_defaults(t);CHECK(!cloud_prepare(t,b,CTL));CHECK(!s->gain);
 t->p[P_GRC]=100;cloud_prepare(t,b,CTL);t->engine=ENGI_PHYS;eng_mem_claim(t);CHECK(!cloud_prepare(t,b,CTL));t->engine=ENGI_GRAIN;eng_mem_claim(t);CHECK(!gr_of(t)->cloud.gain);
 for(unsigned type=1;type<4;type++){t->p[P_GTYPE]=type;t->p[P_GRC]=100;CHECK(!cloud_prepare(t,b,CTL));}
 printf("CLOUD width strict: %llu checks passed; fold exact, gains/rings bounded, decay/reset/engine ownership, no RNG changes\n",(unsigned long long)checks);return 0;
}
