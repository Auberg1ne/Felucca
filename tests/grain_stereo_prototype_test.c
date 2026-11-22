/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_GRAIN_STEREO_PROTOTYPE 1
#define FELUCCA_GRAIN_STEREO_ACCOUNTING 1
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <time.h>
static uint64_t checks, separated, reads, probe_frames;
static long double energy_l,energy_r,energy_diff;
static gr_part_t indexed, p, baseline;
#define CHECK(x) do { checks++; if(!(x)){fprintf(stderr,"stereo prototype %u: %s\n",__LINE__,#x);return 1;} } while(0)
static int kernel(unsigned slot,unsigned n) {
 baseline=p;uint32_t rng=gr_rng[0];
 struct{int32_t pre,m[CTL],post;}mid={.pre=12345,.post=67890},side=mid,mono=mid;
 gr_test_decode_calls=0;int a=gr_run(&baseline,&baseline.g[slot],mono.m,n);unsigned count=gr_test_decode_calls;
 gr_test_decode_calls=0;int b=gr_run_stereo_probe(&p,&p.g[slot],mid.m,side.m,n);
 CHECK(a==b&&gr_test_decode_calls==count&&gr_rng[0]==rng);reads+=count;
 CHECK(!memcmp(&p,&baseline,sizeof p));
 CHECK(mid.pre==12345&&mid.post==67890&&side.pre==12345&&side.post==67890);
 for(unsigned i=0;i<CTL;i++){
  if(i>=n){CHECK(!mid.m[i]&&!side.m[i]);continue;}
  int l=(mid.m[i]+side.m[i])/2,r=(mid.m[i]-side.m[i])/2,x=mono.m[i];
  CHECK(((mid.m[i]+side.m[i])%2)==0&&((mid.m[i]-side.m[i])%2)==0);
  CHECK(abs(l)<=abs(x)&&abs(r)<=abs(x));CHECK((int64_t)l*x>=0&&(int64_t)r*x>=0);
  CHECK(abs(mid.m[i])<=2*abs(x));
  if(!p.pan[slot])CHECK(l==x&&r==x);
  if(p.pan[slot]==64)CHECK(l==0&&r==x);
  if(p.pan[slot]==-64)CHECK(l==x&&r==0);
  separated+=l!=r;probe_frames++;energy_l+=(long double)l*l;energy_r+=(long double)r*r;energy_diff+=(long double)(l-r)*(l-r);
 }
 return 0;
}
int main(void){
 ui_power_on();CHECK(sizeof(gr_grain_t)==64&&sizeof(gr_part_t)==(7592+GR_CLOUD_WIDTH_BYTES)&&sizeof eng_mem==51552);
 CHECK(ENG_GRAIN.render==grain_render);const unsigned sources[]={0,2,3,13,14,5,5,5};
 unsigned admissions=0,rejects=0;
 for(unsigned source=0;source<8;source++){
  if(source>=5){usr_nz[0]=1;usr_zone[0][0]=*gr_zone(13,0);usr_zone[0][0].n=(unsigned[]){32,257,65535}[source-5];}
  for(unsigned ch=0;ch<4;ch++){
   track_t*t=&trk[ch];host_preset(t,ENGI_GRAIN,0);eng_mem_claim(t);t->p[P_E0]=sources[source];t->p[P_GTYPE]=0;
   t->p[P_E1]=64;t->p[P_E2]=0;t->p[P_E4]=0;t->p[P_E5]=40;t->p[P_E6]=0;
   gr_reset(&indexed,sources[source],gr_stamp(sources[source]));for(unsigned j=0;j<GR_NIDX;j++)gr_build(&indexed,t);
   const smp_zone_t*z=gr_zone(sources[source],0);
   for(unsigned width=0;width<=100;width+=25)for(unsigned rev=0;rev<2;rev++)for(unsigned seed=1;seed<=8;seed++){
    p=indexed;gr_test_spread[ch]=width;t->p[P_E6]=rev?127:0;
    vmod_t m={0};m.pitch16=z->root16-576;m.shape=64<<8;gr_rng[ch]=seed*0x9e3779b9u;
    int pending=0;
    for(unsigned slot=0;slot<GR_NG;slot++){
     uint32_t expected=gr_rng[ch];noise32(&expected);noise32(&expected);
     int oldpending=p.pan_pending;uint32_t initial=gr_rng[ch];baseline=p;
     gr_test_spread[ch]=0;gr_test_decode_calls=0;gr_spawn_one(&baseline,t,slot%GR_POLY,0,1000,&m,0,32767);unsigned legacy_reads=gr_test_decode_calls;
     gr_test_spread[ch]=width;gr_rng[ch]=initial;gr_test_decode_calls=0;gr_spawn_one(&p,t,slot%GR_POLY,0,1000,&m,0,32767);
     CHECK(gr_test_decode_calls==legacy_reads&&gr_rng[ch]==expected);
     CHECK(!memcmp(&p,&baseline,(uint8_t*)p.pan-(uint8_t*)&p));
     CHECK(p.g[slot].owner);admissions++;CHECK(abs(p.pan[slot])<=64);
     if(pending){CHECK(p.pan[slot]==oldpending);CHECK(!p.pan_pending&&!p.pan_pair_pending);pending=0;}
     else if(p.pan_pair_pending)pending=1;
     CHECK(p.pan[slot]==-p.pan_pending||!p.pan_pending);
     /* Full pool rejection must preserve pairing and all decoder/head fields. */
     if(slot==GR_NG-1){baseline=p;gr_spawn_one(&p,t,0,0,1000,&m,0,32767);CHECK(!memcmp(&p,&baseline,sizeof p));rejects++;}
    }
    int pan_sum=0;for(unsigned slot=0;slot<GR_NG;slot++)pan_sum+=p.pan[slot];CHECK(pan_sum==0&&!p.pan_pair_pending);
    /* Full-pool, all-owner transport and both intrinsic-side lanes. */
    *gr_of(t)=p;baseline=p;int32_t mids[2][CTL]={{0}},expected[2][CTL]={{0}};
    gr_stereo_clear(gr_of(t));gr_stereo_clear(&baseline);gr_test_decode_calls=0;
    for(unsigned owner=1;owner<=GR_POLY;owner++)grain_stereo_probe_transport(t,owner,mids[owner&1],CTL,owner&1);
    unsigned transport_reads=gr_test_decode_calls;gr_test_decode_calls=0;
    for(unsigned slot=0;slot<GR_NG;slot++){unsigned group=baseline.g[slot].owner&1;gr_run_stereo_probe(&baseline,&baseline.g[slot],expected[group],baseline.stereo_side[group],CTL);}
    CHECK(transport_reads==gr_test_decode_calls);CHECK(!memcmp(mids,expected,sizeof mids));CHECK(!memcmp(&baseline,gr_of(t),sizeof baseline));
    /* Replay every decoder independently, with identical reads/cache state. */
    for(unsigned slot=0;slot<GR_NG;slot++)for(unsigned block=0;block<32&&p.g[slot].owner;block++)CHECK(!kernel(slot,(block%CTL)+1));
    /* Reuse a dirty slot at width0 must overwrite pan and clear pending. */
    memset(p.g,0,sizeof p.g);p.pan[0]=64;p.pan_pending=-64;p.pan_pair_pending=1;gr_test_spread[ch]=0;
    gr_spawn_one(&p,t,0,0,1000,&m,0,32767);CHECK(p.g[0].owner&&p.pan[0]==0&&p.pan_pending==0&&!p.pan_pair_pending);
    /* Unindexed and load-cap rejection cannot mutate latched placement. */
    p=indexed;p.pan[0]=63;p.pan_pending=-63;p.zdone[0]=0;baseline=p;
    gr_spawn_one(&p,t,0,0,1000,&m,0,32767);CHECK(!memcmp(&p,&baseline,sizeof p));rejects++;
    p=indexed;p.g[1].owner=1;p.g[1].step=GR_LOAD_MAX;baseline=p;
    gr_spawn_one(&p,t,0,0,1000,&m,0,32767);CHECK(!memcmp(&p,&baseline,sizeof p));rejects++;
   }
   /* Dry transport uses the same owned lanes, without touching V-SPRD. */
   p=indexed;vmod_t m={0};m.pitch16=z->root16-576;m.shape=64<<8;gr_test_spread[ch]=100;
   gr_spawn_one(&p,t,0,0,1000,&m,0,32767);p.pan[0]=64;*gr_of(t)=p;
   int32_t before[CTL];memcpy(before,SPREAD_BUF(t),sizeof before);
   struct{int32_t pre,mid[CTL],post;}out={.pre=12345,.post=67890};
   grain_stereo_probe_transport(t,1,out.mid,CTL,1);CHECK(out.pre==12345&&out.post==67890);
   grain_stereo_view_t view=track_grain_stereo(t,CTL);CHECK(view.frames==CTL);
   for(unsigned i=0;i<CTL;i++){CHECK(!view.group[0][i]);CHECK(view.group[1][i]==-out.mid[i]);}
   CHECK(!memcmp(before,SPREAD_BUF(t),sizeof before));
   unsigned old=t->engine;t->engine=0;p=*gr_of(t);
   grain_stereo_probe_transport(t,1,out.mid,CTL,1);CHECK(!memcmp(&p,gr_of(t),sizeof p));t->engine=old;
   eng_mem_owner[ch]=0;grain_stereo_probe_transport(t,1,out.mid,CTL,1);CHECK(!memcmp(&p,gr_of(t),sizeof p));eng_mem_owner[ch]=ENGI_GRAIN+1;
   /* Width changes cannot rewrite active grain placement. */
   int saved=gr_of(t)->pan[0];gr_test_spread[ch]=0;CHECK(gr_of(t)->pan[0]==saved);
   p=*gr_of(t);grain_stereo_probe_transport(t,1,out.mid,CTL+1,1);grain_stereo_probe_transport(t,1,out.mid,CTL,2);CHECK(!memcmp(&p,gr_of(t),sizeof p));
  }
 }
 /* Exact hard-pan and signed source extremes with all window anchors. */
 for(unsigned win=0;win<3;win++)for(unsigned len=16;len<=128;len++)for(int pan=-64;pan<=64;pan+=32)for(unsigned sign=0;sign<2;sign++){
  memset(&p,0,sizeof p);gr_grain_t*g=&p.g[0];g->z=gr_zone(0,0);g->a=g->b=sign?-32768:32767;g->gain=32767;g->owner=1;g->left=len;g->winc=UINT32_MAX/len;g->window=(unsigned[]){128,192,255}[win];g->step=0;p.pan[0]=pan;
  while(g->owner)CHECK(!kernel(0,CTL));
 }
 CHECK(separated>10000);CHECK(!gr_test_invalid_reads);printf("Stereo prototype: %llu checks; %u admitted/%u rejected; %llu identical decoder calls; %llu separated channel frames; pan extrema exact 100%% suppression\n",(unsigned long long)checks,admissions,rejects,(unsigned long long)reads,(unsigned long long)separated);
 printf("L/R/difference RMS across %llu probe frames: %.3Lf / %.3Lf / %.3Lf\n",(unsigned long long)probe_frames,sqrtl(energy_l/probe_frames),sqrtl(energy_r/probe_frames),sqrtl(energy_diff/probe_frames));
 return 0;
}
