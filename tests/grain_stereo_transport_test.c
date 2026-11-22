/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"stereo %u: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
 ui_power_on();
 CHECK(sizeof eng_mem==51552 && sizeof(gr_part_t)==7576);
 CHECK(ENG_GRAIN.render==grain_render);
 for(unsigned ch=0;ch<4;ch++) {
  track_t*t=&trk[ch];host_preset(t,ENGI_GRAIN,0);eng_mem_claim(t);
  gr_part_t*p=gr_of(t);
  for(unsigned group=0;group<2;group++)for(unsigned i=0;i<CTL;i++)CHECK(p->stereo_side[group][i]==0);
  CHECK((void*)p->stereo_side[0]!=(void*)SPREAD_BUF(t));
  for(unsigned n=0;n<=CTL;n++) {
   grain_stereo_view_t view=track_grain_stereo(t,n);
   CHECK(view.frames==n && view.group[0]==p->stereo_side[0] && view.group[1]==p->stereo_side[1]);
   for(unsigned group=0;group<2;group++)for(unsigned i=0;i<CTL;i++)p->stereo_side[group][i]=0x123456;
   int32_t out[CTL]={0}; track_render(t,out,n);
   for(unsigned group=0;group<2;group++)for(unsigned i=0;i<CTL;i++)CHECK(p->stereo_side[group][i]==0);
  }
  CHECK(track_grain_stereo(t,CTL+1).group[0]==0);
  vmod_t m={0};int32_t out[CTL+2];for(unsigned i=0;i<CTL+2;i++)out[i]=123;
  grain_render_transport(t,&t->v[0],out+1,CTL+1,&m,0);
  grain_render_transport(t,&t->v[0],out+1,CTL,&m,2);
  for(unsigned i=0;i<CTL+2;i++)CHECK(out[i]==123);
  memset(p->stereo_side,0x55,sizeof p->stereo_side);gr_reset(p,0,gr_stamp(0));
  for(unsigned i=0;i<2*CTL;i++)CHECK(((int32_t*)p->stereo_side)[i]==0);
  unsigned old=t->engine;t->engine=old==0?1:0;
  CHECK(track_grain_stereo(t,CTL).group[0]==0);t->engine=old;
  eng_mem_owner[ch]=0;CHECK(track_grain_stereo(t,CTL).group[0]==0);
  eng_mem_claim(t);CHECK(track_grain_stereo(t,CTL).group[0]!=0);
  for(unsigned i=0;i<2*CTL;i++)CHECK(((int32_t*)p->stereo_side)[i]==0);
 }
 printf("stereo transport: %u checks passed\n",checks);return 0;
}
