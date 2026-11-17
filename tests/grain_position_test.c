/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "audio_stats.h"
#ifdef __SANITIZE_ADDRESS__
#include <sanitizer/asan_interface.h>
#endif
static unsigned checks;
#define REQUIRE(x) do { checks++; if (!(x)) { fprintf(stderr,"position line %u: %s\n",__LINE__,#x); return 1; } } while (0)
static uint64_t head_q16(const gr_head_t *h) { return ((uint64_t)h->sample<<16)|h->frac; }
static uint32_t ref_anchor(uint32_t n,uint32_t target)
{
    if (n<=3) return 0;
    uint64_t c=((uint64_t)(n-1)*(target*65535u/GR_POS_MAX))>>16;
    return c<n-3? (uint32_t)c:n-4;
}
static void source_guard(const smp_zone_t *z,int enable)
{
#ifdef __SANITIZE_ADDRESS__
    __asan_unpoison_memory_region(SMP_DATA,sizeof SMP_DATA);
    if (enable) {
        /* Only factory zones reside in SMP_DATA. User zones use
         * separate storage and must not poison beyond this array. */
        uint64_t end = (uint64_t)z->off + ((uint64_t)z->n + 1u)/2u;
        if (end <= sizeof SMP_DATA) {
            __asan_poison_memory_region(SMP_DATA,z->off);
            __asan_poison_memory_region(SMP_DATA+(size_t)end,
                                        sizeof SMP_DATA-(size_t)end);
        }
    }
#else
    (void)z;(void)enable;
#endif
}
static void user_zone(uint32_t n,uint32_t rate)
{
    usr_nz[0]=1;
    usr_zone[0][0]=(smp_zone_t){.off=16,.n=n,.ls=0,.le=n-1,.rate=rate,.root16=960,.lo=0,.hi=127};
}
static uint32_t reverse_seed(unsigned reverse)
{
    for (uint32_t seed=1;;seed++) {
        int32_t state=(int32_t)seed;uint32_t r=noise32(&state);
        if ((((r>>16)&255u)<127u)==reverse) return seed;
    }
}
static int head_math(void)
{
    const uint32_t lengths[]={1,3,4,8,2048,4099,65535,65536,65539,275098,
        2u*(SMP_USER_POOL_SIZE-SMP_USER_SECTOR_SIZE-SMP_USER_DATA-16u),
        2u*(SMP_USER_POOL_SIZE-SMP_USER_DATA),UINT32_MAX};
    const uint32_t stretch[]={0,1,64,127};
    gr_part_t part={0};
    for (unsigned k=0;k<NELEM(lengths);k++) {
        uint32_t n=lengths[k],domain=n>3?n-3:0;
        for (unsigned pos=0;pos<3;pos++) {
            uint32_t target=(unsigned[]){0,64u<<8,127u<<8}[pos];
            part.head_zl[0]=255;gr_head_retarget(&part.head[0],1,n,target);
            if (!domain) { REQUIRE(part.head_zl[0]==255);continue; }
            REQUIRE(part.head[0].sample==ref_anchor(n,target) && !part.head[0].frac);
            REQUIRE(part.head[0].target==target && part.head[0].sample<domain);
        }
        if (!domain) continue;
        for (unsigned s=0;s<NELEM(stretch);s++) {
            uint32_t move=((65535u>>8)*(pow2_q16(-(int32_t)stretch[s]*10)>>8))*CTL;
            gr_head_t h={.sample=domain-1,.frac=65535,.target=1337};
            uint64_t ref=head_q16(&h),period=(uint64_t)domain<<16;
            for (unsigned b=0;b<2000;b++) {
                ref=(ref+move)%period;gr_head_advance(&h,domain,move);
                REQUIRE(head_q16(&h)==ref && h.sample<domain && h.target==1337);
            }
        }
        /* Walk two complete cycles at nominal speed for every pool-supported domain. */
        if (n!=UINT32_MAX) {
            gr_head_t h={0};uint64_t elapsed=0,period=(uint64_t)domain<<16;
            while (elapsed<period*2) {
                elapsed+=2088960u;gr_head_advance(&h,domain,2088960u);
                REQUIRE(head_q16(&h)==elapsed%period);
            }
        }
        part.head_zl[0]=255;gr_head_retarget(&part.head[0],1,n,0);
        part.head[0].sample=domain/3;part.head[0].frac=12345;
        const uint32_t targets[]={127u<<8,0,64u<<8,(64u<<8)+1,(64u<<8)-1,64u<<8};
        for (unsigned j=0;j<NELEM(targets);j++) {
            int64_t expect=(int64_t)part.head[0].sample+ref_anchor(n,targets[j])-ref_anchor(n,part.head[0].target);
            if (expect<0)expect=0;if ((uint64_t)expect>=domain)expect=domain-1;
            gr_head_retarget(&part.head[0],0,n,targets[j]);
            REQUIRE(part.head[0].sample==(uint32_t)expect && part.head[0].frac==12345);
            gr_head_t before=part.head[0];gr_head_retarget(&part.head[0],0,n,targets[j]);
            REQUIRE(!memcmp(&before,&part.head[0],sizeof before));
        }
    }
    return 0;
}
static int live_position(void)
{
    const unsigned sources[]={0,2,3,13,14};
    const uint32_t stretch[]={0,1,64,127};
    for (unsigned type=0;type<4;type++)for (unsigned si=0;si<NELEM(sources);si++)for (unsigned st=0;st<NELEM(stretch);st++) {
        ui_power_on();track_t *t=&trk[0];t->p[P_E0]=(int16_t)sources[si];t->p[P_GTYPE]=type;t->p[P_GCUT]=0;t->p[108]=(int16_t)stretch[st];
        t->p[P_E1]=0;t->p[P_E2]=80;t->p[P_E5]=0;t->p[P_E6]=0;t->p[P_GB]=type==2?127:0;
        gr_part_t *p=gr_of(t);memset(p,0,sizeof *p);gr_reset(p,sources[si],gr_stamp(sources[si]));
        for (unsigned j=0;j<GR_NIDX;j++)gr_build(p,t);
        voice_t *v=&t->v[0];memset(v,0,sizeof *v);v->note=60;v->active=1;grain_note_on(t,v);
        vmod_t m={0};m.pitch16=960;m.shape=64<<8;m.amp0=m.amp1=20000;
        gr_spawn_one(p,t,0,0,1000,&m,0,32767);REQUIRE(p->g[0].owner==1);
        const smp_zone_t *z=gr_zone(sources[si],0);uint32_t domain=z->n-3;
        /* Active grains age normally; a live change never clears/restarts them. */
        const unsigned positions[]={0,127,0,64};
        for (unsigned j=0;j<NELEM(positions);j++) {
            t->p[P_E1]=(int16_t)positions[j];v->s[1]=100000;
            unsigned left=p->g[0].left;gr_head_t old=p->head[0];
            int32_t hd16 = m.pitch16 - z->root16 + t->p[P_E4] * 16;
            uint32_t hs = (pow2_q16(clamp(hd16, -1536, 576)) >> 8) * (z->rate >> 8);
            if (hs > 65535u) hs = 65535u;
            uint32_t move = ((hs >> 8) * (pow2_q16(-(int32_t)stretch[st]*10) >> 8)) * CTL;
            if (type == 0u) move = (move * (uint32_t)t->p[P_GA]) >> 6;
            uint64_t next=(head_q16(&old)+move)%((uint64_t)domain<<16);
            int64_t expect=(int64_t)(next>>16)+ref_anchor(z->n,positions[j]<<8)-ref_anchor(z->n,old.target);
            if (expect<0)expect=0;if ((uint64_t)expect>=domain)expect=domain-1;
            int32_t out[CTL]={0};grain_render(t,v,out,CTL,&m);
            REQUIRE(p->g[0].owner==1 && p->g[0].left==left-CTL);
            REQUIRE(head_q16(&p->head[0])==next && p->head[0].target==old.target);
            gr_grain_t tail=p->g[0];
            gr_spawn_one(p,t,0,0,1000,&m,0,32767);
            REQUIRE(p->head[0].sample==(uint32_t)expect && p->head[0].frac==(uint16_t)next);
            REQUIRE(!memcmp(&tail,&p->g[0],sizeof tail));
            gr_grain_t *fresh=&p->g[1];
            REQUIRE(fresh->owner==1 && !fresh->rev);
            uint32_t span=(uint32_t)(((uint64_t)fresh->left*fresh->step)>>16);
            uint32_t limit=z->n-3-span;
            REQUIRE(fresh->pos-2==(p->head[0].sample<limit?p->head[0].sample:limit));
            fresh->owner=0; /* dispose only this test probe; the original tail stays active */
        }
        /* The identical effective target supplied through SHP follows identical rules. */
        for (int d=-3;d<=3;d++) {
            gr_head_t old=p->head[0];m.shape=(64<<8)+d;v->s[1]=100000;
            int32_t hd16 = m.pitch16 - z->root16 + t->p[P_E4] * 16;
            uint32_t hs = (pow2_q16(clamp(hd16, -1536, 576)) >> 8) * (z->rate >> 8);
            if (hs > 65535u) hs = 65535u;
            uint32_t move = ((hs >> 8) * (pow2_q16(-(int32_t)stretch[st]*10) >> 8)) * CTL;
            if (type == 0u) move = (move * (uint32_t)t->p[P_GA]) >> 6;
            uint64_t next=(head_q16(&old)+move)%((uint64_t)domain<<16);
            uint32_t target=(64u<<8)+d;
            int64_t expect=(int64_t)(next>>16)+ref_anchor(z->n,target)-ref_anchor(z->n,old.target);
            if (expect<0)expect=0;if ((uint64_t)expect>=domain)expect=domain-1;
            int32_t out[CTL]={0};grain_render(t,v,out,CTL,&m);
            REQUIRE(head_q16(&p->head[0])==next && p->head[0].target==old.target);
            gr_grain_t tail=p->g[0];gr_spawn_one(p,t,0,0,1000,&m,0,32767);
            REQUIRE(p->g[1].owner==1 && !memcmp(&tail,&p->g[0],sizeof tail));
            REQUIRE(p->head[0].sample==(uint32_t)expect && p->head[0].frac==(uint16_t)next);
            p->g[1].owner=0;
        }
        /* A rejected seek-index start must not consume the pending target. */
        t->p[P_E1]=127;m.shape=64<<8;gr_head_t before=p->head[0];uint16_t done=p->zdone[0];p->zdone[0]=1;
        gr_spawn_one(p,t,0,0,1000,&m,0,32767);
        REQUIRE(!p->g[1].owner && !memcmp(&before,&p->head[0],sizeof before));
        p->zdone[0]=done;gr_spawn_one(p,t,0,0,1000,&m,0,32767);
        REQUIRE(p->g[1].owner==1 && p->head[0].target==GR_POS_MAX);p->g[1].owner=0;
        /* SRC changes to another factory's zone 0 invalidate even the unchanged zone tag. */
        unsigned nextsrc=sources[(si+1)%NELEM(sources)];t->p[P_E0]=(int16_t)nextsrc;grain_block(t);
        REQUIRE(p->head_zl[0]==255 && !p->g[0].owner && v->active);
        for (unsigned j=0;j<GR_NIDX;j++)gr_build(p,t);
        gr_spawn_one(p,t,0,0,1000,&m,0,32767);
        REQUIRE(p->head_zl[0]==0 && p->head[0].sample==ref_anchor(gr_zone(nextsrc,0)->n,gr_head_target(t,&m)) && !p->head[0].frac);
    }
    return 0;
}
static int boundaries(void)
{
    const unsigned sources[]={0,2,3,13,14,5};
    const uint32_t sizes[]={8,19,35,2048,65535,65536,65539,275098,275200,275201,
        2u*(SMP_USER_POOL_SIZE-SMP_USER_SECTOR_SIZE-SMP_USER_DATA-16u)};
    unsigned guarded=0,sounding=0,limited=0;
    for (unsigned src=0;src<NELEM(sources);src++)for (unsigned nz=0;nz<(sources[src]==5?NELEM(sizes):1u);nz++) {
        source_guard(&SMP_ZONES[0],0);ui_power_on();track_t *t=&trk[0];t->p[P_E0]=(int16_t)sources[src];t->p[P_GCUT]=0;
        if (sources[src]==5) {
            user_zone(sizes[nz],65792);smp_user_hdr_t h={0};h.magic=SMP_USER_MAGIC;h.version=1;h.nz=1;h.zone[0]=usr_zone[0][0];
            h.data_len=16+(sizes[nz]+1)/2;
            REQUIRE(smp_user_header_valid(&h,SMP_USER_POOL_SIZE-SMP_USER_SECTOR_SIZE-SMP_USER_DATA));
        }
        gr_part_t *p=gr_of(t);memset(p,0,sizeof *p);gr_reset(p,sources[src],gr_stamp(sources[src]));
        for (unsigned i=0;i<GR_NIDX;i++)gr_build(p,t);
        static gr_part_t cache;cache=*p;
        const smp_zone_t *z=gr_zone(sources[src],0);
        for (unsigned type=0;type<4;type++)for (unsigned size=0;size<2;size++)for (unsigned hold=0;hold<2;hold++)
            for (unsigned rev=0;rev<2;rev++)for (unsigned pos=0;pos<3;pos++) {
                *p=cache;voice_t *v=&t->v[0];memset(v,0,sizeof *v);v->active=1;v->s[0]=0;v->s[1]=1000000;
                t->p[P_GTYPE]=type;t->p[P_GA]=hold?127:0;t->p[P_GB]=type==2?127:64;
                t->p[P_E2]=size?127:0;t->p[P_E4]=size?24:-1;t->p[P_E6]=127;t->p[P_E1]=(int16_t)((unsigned[]){0,64,127}[pos]);
                gr_rng[0]=(int32_t)reverse_seed(rev);vmod_t m={0};m.pitch16=960;m.shape=64<<8;m.amp0=m.amp1=16384;
                source_guard(z,1);gr_spawn_one(p,t,0,0,1000,&m,0,32767);
                gr_grain_t *g=&p->g[0];
                if (!p->zcnt[0]) { REQUIRE(!g->owner);limited++;source_guard(z,0);continue; }
                unsigned len=(441u*pow2_q16(t->p[P_E2]*8))>>16;
                if (type==1)len=len*(128u+(unsigned)t->p[P_GA]*3u)/128u;
                if (!g->owner) { REQUIRE(z->n<40);source_guard(z,0);continue; }
                REQUIRE(g->rev==rev && g->left>=16);
                uint64_t span=((uint64_t)g->left*g->step+(g->rev?65535u:0u))>>16;
                REQUIRE(span<=z->n-3);
                uint32_t start=g->rev?g->pos:g->pos-2;
                REQUIRE(g->rev?(start>=span && start+1<z->n):(start+span+2<z->n));
                /* Independent exact quotient: non-aligned user rates cannot over-fit. */
                uint64_t fullspan=((uint64_t)len*g->step+(g->rev?65535u:0u))>>16;
                unsigned expect=fullspan>z->n-3?(unsigned)(((uint64_t)(z->n-3)<<16)/g->step):len;
                REQUIRE(g->left==expect);
                audio_stats_t stats={0};unsigned rendered=0;
                while (g->owner) {
                    rendered+=g->left<CTL?g->left:CTL;
                    int32_t out[CTL]={0};grain_render(t,v,out,CTL,&m);
                    uint64_t steps=((uint64_t)rendered*g->step+(g->rev?65535u:0u))>>16;
                    REQUIRE(g->pos==(g->rev?start-steps:start+2+steps));
                    for (unsigned i=0;i<CTL;i++)audio_stats_push(&stats,out[i]);
                    REQUIRE(g->pos<z->n);
                }
                if (audio_stats_sounding(&stats,1,0.1))sounding++;
                guarded++;source_guard(z,0);
            }
    }
    printf("position boundaries: %u guarded grains, %u sounding, %u unchanged index-capacity refusals\n",guarded,sounding,limited);
    return 0;
}
static int four_parts(void)
{
    ui_power_on();song.master_q12=4096;
    const unsigned sources[]={0,2,3,13};
    for (unsigned ch=0;ch<4;ch++) {
        track_t *t=&trk[ch];host_preset(t,ENGI_GRAIN,0);t->p[P_E0]=(int16_t)sources[ch];t->p[P_GTYPE]=(int16_t)ch;
        t->p[P_LEVEL]=100;t->p[P_MUTE]=t->p[P_ATK]=t->p[P_DEC]=t->p[P_REL]=0;t->p[P_SUS]=127;
        t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=t->p[P_DIST]=t->p[P_ITYPE]=0;t->p[P_GCUT]=0;
        t->p[P_ED_SHP]=t->p[P_LD_SHP]=0;
        gr_part_t *p=gr_of(t);memset(p,0,sizeof *p);gr_reset(p,sources[ch],gr_stamp(sources[ch]));
        for (unsigned i=0;i<GR_NIDX;i++)gr_build(p,t);
        trk_note_on(t,60+2*ch,100);
    }
    audio_stats_t stats={0};
    for (unsigned b=0;b<1024;b++) { int32_t out[CTL*2];mix_block(out,CTL);for(unsigned i=0;i<CTL*2;i++)audio_stats_push(&stats,out[i]); }
    REQUIRE(audio_stats_sounding(&stats,32,1));for (unsigned ch=0;ch<4;ch++)REQUIRE(home_level_read(ch)>0);
    static gr_part_t untouched[3];int32_t random[3];
    for (unsigned ch=1;ch<4;ch++) { untouched[ch-1]=*gr_of(&trk[ch]);random[ch-1]=gr_rng[ch]; }
    vmod_t m={0};m.pitch16=960;m.shape=64<<8;m.amp0=m.amp1=20000;
    trk[0].p[P_E1]=127;gr_spawn_one(gr_of(&trk[0]),&trk[0],0,0,1000,&m,0,32767);
    REQUIRE(gr_of(&trk[0])->head[0].target==GR_POS_MAX);
    for (unsigned ch=1;ch<4;ch++)REQUIRE(!memcmp(&untouched[ch-1],gr_of(&trk[ch]),sizeof(gr_part_t)) && random[ch-1]==gr_rng[ch]);
    printf("four parts: distinct factory/personality tracks sounding; channel-local retarget leaves other pools/RNG untouched\n");
    return 0;
}
/* Allow later source-head tests to reuse the guarded fixtures without a second main. */
#ifndef GRAIN_POSITION_NO_MAIN
int main(void)
{
    REQUIRE(sizeof(gr_head_t)==8 && sizeof(gr_part_t)<=ENG_MEM_PART);
    if (head_math() || live_position() || boundaries() || four_parts())return 1;
    printf("GRAIN position: %u checks passed; state=%zu / %u bytes, head=%zu bytes; F2/F3/F6\n",checks,sizeof(gr_part_t),ENG_MEM_PART,sizeof(gr_head_t));
    return 0;
}

#endif
