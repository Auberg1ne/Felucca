/* SPDX-License-Identifier: GPL-3.0-only */
/* Fixed decorrelation, not per-grain stereo. S is added with opposite signs:
 * the pre-master fold is unchanged. No moving taps, pitch modulation or RNG. */
#define CW_A 31u
#define CW_B 79u
#define CW_MAX 4096 /* Q14, at most 1/4 of the difference of two unity allpasses */
typedef struct {
    int32_t ring[CW_A+CW_B], gain;
    uint16_t a,b;
    uint32_t epoch;
    int32_t side[CTL];
} cloud_width_t;
_Static_assert(sizeof(cloud_width_t)==580,"owned CLOUD decorrelator");
static uint32_t cloud_width_epoch[NPART];
static int32_t cw_narrow(int64_t x){return x>INT32_MAX?INT32_MAX:x<INT32_MIN?INT32_MIN:(int32_t)x;}
/* mode0: fixed-delay difference; mode1: Schroeder allpass difference, a=1/2.
 * Explicit mode is for isolated probes; production calls only mode1. */
static int32_t cw_step_gain(cloud_width_t*s,int32_t x,int32_t target,uint32_t mode,uint32_t slew)
{
    target=clamp(target,0,CW_MAX);
    s->gain+=clamp(target-s->gain,-(int32_t)slew,(int32_t)slew); /* <=256 frames, about5.8ms */
    if(!s->gain&&!target){
        memset(s->ring,0,sizeof s->ring);s->a=s->b=0;
        return 0;
    }
    int32_t ya=s->ring[s->a],yb=s->ring[CW_A+s->b];
    if(mode){
        ya=cw_narrow((int64_t)ya-x/2);yb=cw_narrow((int64_t)yb-x/2);
        s->ring[s->a]=cw_narrow((int64_t)x+ya/2);s->ring[CW_A+s->b]=cw_narrow((int64_t)x+yb/2);
    }else{s->ring[s->a]=x;s->ring[CW_A+s->b]=x;}
    if(++s->a==CW_A)s->a=0;if(++s->b==CW_B)s->b=0;
    int32_t side=(int32_t)(((int64_t)ya-yb)*s->gain/16384);
    if(!target&&s->gain==0){memset(s->ring,0,sizeof s->ring);s->a=s->b=0;}
    return side;
}
/* Convenience for probes. Production computes this target once per block. */
static int32_t cw_step(cloud_width_t*s,int32_t x,int32_t c,uint32_t mode,uint32_t slew)
{
    return cw_step_gain(s,x,clamp(c,0,100)*CW_MAX/100,mode,slew);
}
/* Main/control boundary only; audio owns the ring and consumes the epoch. */
static void cloud_width_request_reset(const track_t*t)
{
    uintptr_t p=(uintptr_t)t,base=(uintptr_t)trk;
    if(p>=base&&p<base+sizeof trk&&(p-base)%sizeof(track_t)==0)cloud_width_epoch[(p-base)/sizeof(track_t)]++;
}
