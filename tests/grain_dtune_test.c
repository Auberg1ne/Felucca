/* SPDX-License-Identifier: GPL-3.0-only */
#define GRAIN_POSITION_NO_MAIN 1
#include "grain_position_test.c"
#undef REQUIRE
#define REQUIRE(x) do { checks++;if(!(x)){fprintf(stderr,"DTUNE line %u: %s\n",__LINE__,#x);return 1;} } while(0)
static void prepare(unsigned src,unsigned type,unsigned detune)
{
    source_guard(&SMP_ZONES[0],0);ui_power_on();track_t *t=&trk[0];host_preset(t,ENGI_GRAIN,0);
    t->p[P_E0]=src;t->p[P_GTYPE]=type;t->p[P_GA]=64;t->p[P_GB]=64;
    t->p[P_VOICE]=V_UNISON;t->p[P_DETUNE]=detune;t->p[P_E7]=127;t->p[P_GCUT]=0;
    t->p[P_E1]=64;t->p[P_E2]=80;t->p[P_E5]=t->p[P_E6]=0;t->p[P_PRIO]=0;
    if(src==5)user_zone(65539,4u<<16);
    gr_part_t *p=gr_of(t);memset(p,0,sizeof *p);gr_reset(p,src,gr_stamp(src));
    for(unsigned i=0;i<GR_NIDX;i++)gr_build(p,t);
    trk_note_on(t,60,100);
}
static int allocator(void)
{
    const unsigned values[]={0,1,40,64,127};
    REQUIRE(P_DETUNE==44 && GR_POLY==3 && NVOICE==8);
    for(unsigned type=0;type<4;type++)for(unsigned d=0;d<NELEM(values);d++) {
        prepare(3,type,values[d]);track_t *t=&trk[0];
        int outer=7*(int)values[d]*56/889;
        for(unsigned vi=0;vi<3;vi++) {
            REQUIRE(t->v[vi].active && t->v[vi].fine==((int)vi-1)*outer);
            REQUIRE(t->v[vi].side==(vi&1)); /* keep actual allocator stereo, not invented panning */
        }
        /* A held edit changes the parameter only. A winning note event recalculates fine. */
        int saved=t->v[0].fine;t->p[P_DETUNE]=127;
        for(unsigned vi=0;vi<3;vi++)t->v[vi].env=1000; /* held envelope; no fresh-note clearing */
        vmod_t m={0};m.pitch16=960;m.shape=64<<8;m.amp0=m.amp1=20000;
        for(unsigned b=0;b<4;b++){int32_t out[CTL]={0};grain_render(t,&t->v[0],out,CTL,&m);}
        REQUIRE(t->v[0].fine==saved);
        gr_grain_t old[GR_NG];memcpy(old,gr_of(t)->g,sizeof old);
        trk_note_on(t,62,100);REQUIRE(!memcmp(old,gr_of(t)->g,sizeof old));REQUIRE(t->v[0].fine==-56 && t->v[1].fine==0 && t->v[2].fine==56);
        trk_note_off(t,62);REQUIRE(t->v[0].fine==-56); /* held-stack fallback uses same allocator */
    }
    return 0;
}
static int rates(void)
{
    const unsigned sources[]={0,2,3,13,14,5},detunes[]={0,1,40,64,127};
    unsigned admitted=0,capped=0;uint32_t measured[5][3]={{0}};
    for(unsigned type=0;type<4;type++)for(unsigned si=0;si<NELEM(sources);si++) {
        prepare(sources[si],type,0);track_t *t=&trk[0];gr_part_t *p=gr_of(t);const smp_zone_t *z=gr_zone(sources[si],0);
        source_guard(z,1);
        for(unsigned d=0;d<NELEM(detunes);d++) {
            t->p[P_DETUNE]=detunes[d];mono_play(t,60,100,1,0); /* actual allocator offsets */
            for(unsigned pt=0;pt<3;pt++)for(unsigned rnd=0;rnd<2;rnd++)for(unsigned rev=0;rev<2;rev++)for(unsigned interval=0;interval<2;interval++) {
                t->p[P_E4]=(int[]){-24,0,24}[pt];t->p[P_E6]=rnd?127:0;t->p[P_E1]=rev?0:127;
                t->p[108]=pt==1?127:0;t->p[P_GA]=type==0?(rev?127:0):64;
                int32_t extra=type==3?(7+interval*12)*16:0;
                if(!rnd && rev)continue;
                for(unsigned vi=0;vi<3;vi++) {
                    memset(p->g,0,sizeof p->g);p->head_zl[vi]=255;gr_rng[0]=reverse_seed(rnd?rev:0);
                    int32_t random=gr_rng[0];uint32_t r1=noise32(&random);
                    int amt=t->p[P_E6]*t->p[P_E6]*192/(127*127);
                    int d16=t->p[P_E4]*16+extra+(((int32_t)(r1&65535)-32768)*amt>>15);
                    uint64_t raw=(uint64_t)(pow2_q16(clamp(d16,-1536,576))>>8)*(z->rate>>8);
                    uint64_t scaled=raw*(4096+t->v[vi].fine)/4096;
                    uint32_t expected=scaled>GR_STEP_MAX?GR_STEP_MAX:scaled<256?256:(uint32_t)scaled;
                    vmod_t m={0};m.pitch16=z->root16;m.shape=64<<8;m.amp0=m.amp1=20000;m.fine=777; /* unrelated residual must not double-detune */
                    gr_spawn_one(p,t,vi,0,1000,&m,extra,32767);gr_grain_t *g=&p->g[0];
                    REQUIRE(g->owner==vi+1 && g->step==expected && g->rev==(rnd?rev:0));
                    REQUIRE(g->step>=256 && g->step<=GR_STEP_MAX);admitted++;capped+=scaled>GR_STEP_MAX;
                    uint64_t span=((uint64_t)g->left*g->step+(g->rev?65535:0))>>16;
                    uint32_t start=g->rev?g->pos:g->pos-2;
                    REQUIRE(g->rev?(start>=span && start+1<z->n):(start+span+2<z->n));
                    if(type==0 && si==0 && pt==1 && !rnd && !interval)measured[d][vi]=g->step;
                    gr_head_t before=p->head[vi];g->left=g->left<CTL?g->left:CTL;
                    t->v[vi].s[1]=100000;int32_t out[CTL+2]={0};out[0]=123;out[CTL+1]=-123;
                    grain_render(t,&t->v[vi],out+1,CTL,&m);
                    REQUIRE(out[0]==123 && out[CTL+1]==-123 && p->g[0].pos<z->n);
                    /* Scanning depends on pitch/STRETCH/DRIFT, never the fine voice term. */
                    uint32_t hs=(pow2_q16(clamp(t->p[P_E4]*16,-1536,576))>>8)*(z->rate>>8);if(hs>65535)hs=65535;
                    uint64_t move=(uint64_t)(hs>>8)*(pow2_q16(-(int)t->p[108]*10)>>8)*CTL;
                    if(type==0)move=move*(unsigned)t->p[P_GA]/64;
                    REQUIRE(head_q16(&p->head[vi])==(head_q16(&before)+move)%((uint64_t)(z->n-3)<<16));
                }
            }
        }
        source_guard(z,0);
    }
    for(unsigned d=0;d<5;d++) {
        printf("DTUNE=%u fine=[%d,0,%d] uncapped-steps=[%u,%u,%u]\n",detunes[d],-7*(int)detunes[d]*56/889,7*(int)detunes[d]*56/889,measured[d][0],measured[d][1],measured[d][2]);
        REQUIRE(measured[d][1]==measured[0][1]);
        if(d>1)REQUIRE(measured[d][0]<measured[d-1][0] && measured[d][2]>measured[d-1][2]);
    }
    printf("DTUNE rate cases: %u admitted, %u upper-cap results\n",admitted,capped);
    return 0;
}
static int short_boundaries(void)
{
    const unsigned sizes[]={19,35,4099},values[]={0,40,127};unsigned admitted=0,refused=0;
    for(unsigned type=0;type<4;type++)for(unsigned si=0;si<NELEM(sizes);si++) {
        prepare(5,type,0);track_t *t=&trk[0];gr_part_t *p=gr_of(t);
        user_zone(sizes[si],65536);gr_reset(p,5,gr_stamp(5));for(unsigned i=0;i<GR_NIDX;i++)gr_build(p,t);
        const smp_zone_t *z=gr_zone(5,0);source_guard(z,1);
        t->p[P_E2]=127;t->p[P_GA]=type==0?64:127;t->p[P_GB]=127;t->p[P_E6]=127;
        for(unsigned d=0;d<NELEM(values);d++) {
            t->p[P_DETUNE]=values[d];mono_play(t,60,100,1,0);
            for(unsigned vi=0;vi<3;vi++)for(unsigned reverse=0;reverse<2;reverse++)for(unsigned pos=0;pos<2;pos++) {
                memset(p->g,0,sizeof p->g);p->head_zl[vi]=255;t->p[P_E1]=pos?127:0;
                gr_rng[0]=reverse_seed(reverse);vmod_t m={0};m.pitch16=z->root16;m.shape=64<<8;m.amp0=m.amp1=20000;
                gr_spawn_one(p,t,vi,0,1000,&m,0,32767);gr_grain_t *g=&p->g[0];
                if(!g->owner){REQUIRE(z->n<40);refused++;continue;}
                admitted++;t->v[vi].s[1]=1000000;unsigned frames=0,start=g->rev?g->pos:g->pos-2;uint32_t initial=g->left;
                while(g->owner) {
                    unsigned count=g->left<CTL?g->left:CTL;frames+=count;
                    int32_t out[CTL+2]={0};out[0]=123;out[count+1]=-123;
                    grain_render(t,&t->v[vi],out+1,count,&m);
                    uint64_t steps=((uint64_t)frames*g->step+(g->rev?65535:0))>>16;
                    REQUIRE(g->pos==(g->rev?start-steps:start+2+steps) && g->pos<z->n);
                    REQUIRE(out[0]==123 && out[count+1]==-123);
                }
                REQUIRE(frames==initial);
            }
        }
        source_guard(z,0);
    }
    printf("DTUNE short full-grain boundaries: %u admitted, %u retained minimum-length refusals\n",admitted,refused);
    return 0;
}
int main(void)
{
    if(allocator() || rates() || short_boundaries())return 1;
    printf("GRAIN DTUNE: %u checks passed; allocator lifecycle, wide rate reference, boundaries and scan independence\n",checks);
    return 0;
}
