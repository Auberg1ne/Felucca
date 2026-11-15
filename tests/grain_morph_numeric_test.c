/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static uint64_t checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"morph numeric %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int32_t reference(unsigned m,uint32_t phase){
 if(m>127 || m==64)return gr_win(phase);
 if(!m)return gr_gauss_win(phase);if(m==127)return gr_tukey_win(phase);
 int64_t a=m<64?gr_gauss_win(phase):gr_win(phase),b=m<64?gr_win(phase):gr_tukey_win(phase);
 unsigned d=m<64?64:63,n=m<64?m:m-64;return (int32_t)((a*(d-n)+b*n+d/2)/d);
}
int main(void){
 /* Exhaust anchors across every effective Q32 position and low-bit endpoint. */
 for(unsigned i=0;i<256;i++)for(unsigned f=0;f<32768;f++){
  uint32_t p=(i<<24)|(f<<9);
  CHECK(gr_morph_win(0,p)==gr_gauss_win(p));CHECK(gr_morph_win(64,p)==gr_win(p));CHECK(gr_morph_win(127,p)==gr_tukey_win(p));
  CHECK(gr_morph_win(0,p|511)==gr_gauss_win(p));CHECK(gr_morph_win(64,p|511)==gr_win(p));CHECK(gr_morph_win(127,p|511)==gr_tukey_win(p));
 }
 /* All morphs, all table intervals, difficult fractions and phase endpoints. */
 const unsigned fractions[]={0,1,8191,16383,16384,32766,32767};
 for(unsigned m=0;m<128;m++)for(unsigned i=0;i<256;i++)for(unsigned k=0;k<NELEM(fractions);k++){
  uint32_t p=(i<<24)|(fractions[k]<<9)|511u;int32_t v=gr_morph_win(m,p);CHECK(v==reference(m,p));CHECK(v>=0 && v<=32767);
 }
 for(unsigned m=0;m<128;m++)for(int32_t a=0;a<=32767;a++)for(unsigned c=0;c<4;c++){
  int32_t b=c==0?0:c==1?32767:c==2?a:32767-a;unsigned d=m<64?64:63,n=m<64?m:m-64;
  int32_t ref=(int32_t)(((int64_t)a*(d-n)+(int64_t)b*n+d/2)/d),got=gr_morph_mix(m,a,b);
  CHECK(got==ref);CHECK(got>=(a<b?a:b) && got<=(a>b?a:b));
 }
 uint32_t r=1;
 for(unsigned i=0;i<1000000;i++){r=r*1664525u+1013904223u;unsigned m=(r>>24)&127;CHECK(gr_morph_win(m,r)==reference(m,r));}
 for(unsigned m=128;m<=255;m++)CHECK(gr_morph_win(m,UINT32_MAX)==gr_win(UINT32_MAX));
 CHECK(gr_morph_win(UINT32_MAX,0x80000000)==32767);
 printf("morph numerical: %llu checks passed; all controls, exhaustive effective anchor phases, wide blend bounds\n",(unsigned long long)checks);return 0;
}
