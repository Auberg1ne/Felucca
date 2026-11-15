/* SPDX-License-Identifier: GPL-3.0-only */
/* Reuse guarded source fixtures and independent wide head references, not DSP. */
#define GRAIN_POSITION_NO_MAIN 1
#include "grain_position_test.c"
#undef REQUIRE
#define REQUIRE(x) do { checks++; if (!(x)) { fprintf(stderr,"DRIFT line %u: %s\n",__LINE__,#x);return 1;} } while (0)
static void prepare(unsigned src)
{
    ui_power_on();track_t *t=&trk[0];host_preset(t,ENGI_GRAIN,0);
    t->p[P_E0]=src;t->p[P_E7]=127;t->p[P_GCUT]=0;t->p[P_GTYPE]=0;t->p[P_GA]=64;
    gr_part_t *p=gr_of(t);memset(p,0,sizeof *p);gr_reset(p,src,gr_stamp(src));
    for(unsigned i=0;i<GR_NIDX;i++)gr_build(p,t);
    voice_t *v=&t->v[0];memset(v,0,sizeof *v);v->note=60;v->active=1;grain_note_on(t,v);
}
static int arithmetic(void)
{
    prepare(13);track_t *t=&trk[0];voice_t *v=&t->v[0];gr_part_t *p=gr_of(t);
    const smp_zone_t *z=gr_zone(13,0);uint64_t period=(uint64_t)(z->n-3)<<16;
    p->head_zl[0]=0;p->head[0]=(gr_head_t){.sample=z->n-4,.frac=65535,.target=64<<8};
    vmod_t m={0};m.pitch16=z->root16+384;m.shape=64<<8;m.amp0=m.amp1=32767;
    REQUIRE(CTL==32 && P_GA==104); /* maximum product proof uses the actual fixed block bound */
    for(unsigned stretch=0;stretch<128;stretch++) {
        t->p[108]=stretch;uint32_t f=pow2_q16(-(int)stretch*10)>>8;REQUIRE(f<=256);
        for(unsigned drift=0;drift<128;drift++)for(unsigned n=0;n<=CTL;n++) {
            t->p[P_GA]=drift;t->p[P_GB]=(drift%3)*63;t->p[P_E5]=127-drift;
            uint64_t base=(uint64_t)255*f*n,move=base*drift/64;
            REQUIRE(base*drift<UINT32_MAX);
            uint64_t expect=(head_q16(&p->head[0])+move)%period;
            v->s[1]=100000;int32_t out[CTL+2]={0};out[0]=1234;out[n+1]=-1234;
            grain_render(t,v,out+1,n,&m);
            REQUIRE(head_q16(&p->head[0])==expect && p->head[0].target==(64<<8));
            REQUIRE(out[0]==1234 && out[n+1]==-1234);
            if(drift==0)REQUIRE(move==0);if(drift==64)REQUIRE(move==base);
            if(drift>0 && n)REQUIRE(move>base*(drift-1)/64);
        }
    }
    /* Full-length wrapping at maximum speed, short/legal user domains included.
     * Index-limited user zones are head-arithmetic tests, not a playability claim. */
    const uint32_t lengths[]={8,65535,65536,65539,275098,482272};
    for(unsigned j=0;j<NELEM(lengths);j++) {
        gr_head_t h={.sample=lengths[j]-4,.frac=12345};uint64_t ref=head_q16(&h),period=(uint64_t)(lengths[j]-3)<<16;
        for(unsigned b=0;b<20000;b++) {
            uint64_t move=(uint64_t)255*256*CTL*127/64;ref=(ref+move)%period;
            gr_head_advance(&h,(uint32_t)(period>>16),(uint32_t)move);REQUIRE(head_q16(&h)==ref);
        }
    }
    return 0;
}
static int live(void)
{
    const unsigned sources[]={0,2,3,13,14};
    for(unsigned si=0;si<NELEM(sources);si++)for(unsigned stretch=0;stretch<4;stretch++) {
        prepare(sources[si]);track_t *t=&trk[0];voice_t *v=&t->v[0];gr_part_t *p=gr_of(t);
        t->p[108]=(int[]){0,1,64,127}[stretch];t->p[P_GA]=t->p[P_E1]=t->p[P_E5]=t->p[P_E6]=t->p[P_GB]=0;
        vmod_t m={0};m.pitch16=960;m.shape=64<<8;m.amp0=m.amp1=20000;
        const smp_zone_t *z=gr_zone(sources[si],0);
        gr_spawn_one(p,t,0,0,1000,&m,0,32767);REQUIRE(p->g[0].owner==1);
        source_guard(z,1);
        const uint32_t targets[]={127u<<8,0,64u<<8,(64u<<8)+3,(64u<<8)-3};
        for(unsigned j=0;j<NELEM(targets);j++) {
            gr_head_t before=p->head[0];gr_grain_t old=p->g[0];
            t->p[P_E1]=j<3?targets[j]>>8:64;m.shape=j<3?64<<8:(int32_t)targets[j]; /* SHP both directions at fixed POS */
            v->s[1]=100000;int32_t out[CTL]={0};grain_render(t,v,out,CTL,&m);
            REQUIRE(!memcmp(&before,&p->head[0],sizeof before));
            REQUIRE(p->g[0].owner==old.owner && p->g[0].left==old.left-CTL);
            old=p->g[0];gr_spawn_one(p,t,0,0,1000,&m,0,32767);
            REQUIRE(p->head[0].target==targets[j] && p->head[0].sample==ref_anchor(z->n,targets[j]));
            REQUIRE(!memcmp(&old,&p->g[0],sizeof old));p->g[1].owner=0;
        }
        /* Live speed/TYPE changes preserve the grain's cursor/step/window law. */
        const unsigned types[]={0,1,2,3,0},values[]={127,1,127,96,0};
        for(unsigned j=0;j<NELEM(types);j++) {
            t->p[P_GTYPE]=types[j];t->p[P_GA]=values[j];t->p[P_GB]=127;
            gr_grain_t old=p->g[0],expect=old;int32_t scratch[CTL]={0};gr_run(p,&expect,scratch,CTL);
            v->s[1]=100000;int32_t out[CTL]={0};grain_render(t,v,out,CTL,&m);
            REQUIRE(!memcmp(&expect,&p->g[0],sizeof expect));
        }
        /* Fast scan through a guarded wrap with both forward/reverse grains. */
        t->p[P_GTYPE]=0;t->p[P_GA]=127;t->p[P_E6]=127;t->p[P_E1]=64;m.shape=64<<8;
        p->head[0].sample=z->n-4;p->head[0].frac=65535;v->s[1]=0;
        for(unsigned b=0;b<512;b++) {
            int32_t out[CTL]={0};grain_render(t,v,out,CTL,&m);
            REQUIRE(p->head[0].sample<z->n-3);
            for(unsigned g=0;g<GR_NG;g++)if(p->g[g].owner)REQUIRE(p->g[g].pos<p->g[g].z->n);
        }
        source_guard(z,0);
    }
    return 0;
}
static int spawn_isolation(void)
{
    const unsigned sources[]={0,2,3,13,14};
    for(unsigned si=0;si<NELEM(sources);si++)for(unsigned width=0;width<3;width++)for(unsigned spread=0;spread<3;spread++) {
        prepare(sources[si]);track_t *t=&trk[0];gr_part_t *p=gr_of(t);
        t->p[P_GB]=(int[]){0,64,127}[width];t->p[P_E5]=(int[]){0,64,127}[spread];t->p[P_E6]=0;
        vmod_t m={0};m.pitch16=960;m.shape=64<<8;m.amp0=m.amp1=20000;
        gr_grain_t expected={0};int32_t rng=0;
        for(unsigned j=0;j<3;j++) {
            memset(p->g,0,sizeof p->g);p->head_zl[0]=255;t->p[P_GA]=(int[]){0,64,127}[j];gr_rng[0]=12345;
            gr_spawn_one(p,t,0,0,1000,&m,0,32767);REQUIRE(p->g[0].owner==1);
            if(!j){expected=p->g[0];rng=gr_rng[0];}
            else REQUIRE(!memcmp(&expected,&p->g[0],sizeof expected) && rng==gr_rng[0]);
        }
    }
    return 0;
}
int main(void)
{
    if(arithmetic() || live() || spawn_isolation())return 1;
    printf("CLOUD DRIFT: %u checks passed; full-range wide reference, fractional wraps, zero-speed live POS/SHP and untouched grain cursors\n",checks);
    return 0;
}
