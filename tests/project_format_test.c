/* SPDX-License-Identifier: GPL-3.0-only */
/* FUN11 storage, independent FUN10 fixtures, and historical source migration. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "old_pack.h"
#include <assert.h>

static unsigned checks;
#define REQUIRE(x) do { checks++; if (!(x)) { fprintf(stderr, "line %u: %s\n", __LINE__, #x); exit(1); } } while (0)
static void seal(project_store_t *s)
{
    uint32_t h = proj_hash(s->raw, sizeof s->raw - 4);
    memcpy(s->raw + sizeof s->raw - 4, &h, 4);
}
static void fixture(project_t *q)
{
    memset(q, 0, sizeof *q); q->magic = PROJ_MAGIC; q->size = sizeof *q;
    q->parts = NPART; q->phys = PROJ_PHYS; q->sel = 2;
    for (unsigned t = 0; t < NTRK; t++) {
        q->t[t].engine = ENGI_GRAIN;
        for (unsigned p = 0; p < P_COUNT; p++) q->t[t].p[p] = (p * 19 + t * 31) % 192 - 64;
        for (unsigned i = 0; i < FM6_PACKED; i++) q->fm6[t][i] = (i * 17 + t * 29) & 127;
        for (unsigned i = 0; i < NSTEP; i++) {
            step_t *s = &q->t[t].step[i]; s->n = 4; s->time = i % 3; s->vel = 127;
            for (unsigned j = 0; j < 4; j++) s->note[j] = (i + j) & 127;
            s->hit = 255; s->acc = 85; step_set_chance(s, 100);
            step_set_ratchet(s, 1 + i % 4); step_set_nudge(s, i % 16 - 8);
        }
    }
    q->g[G_ROUTE] = ROUTE_TRACK;
    q->mch[0] = 4; q->mch[1] = 15; q->mch[2] = 16; q->mch[3] = 17;
    memcpy(q->name, "FULL PROJECT", 12);
    q->chain.count = CHAIN_ROWS;
    for (unsigned i = 0; i < CHAIN_ROWS; i++) {
        for (unsigned t = 0; t < NTRK; t++) q->chain.row[i].slot[t] = (i + t) % 5;
        q->chain.row[i].repeat = 1 + i;
    }
    q->motion.count = MOTION_MAX; q->motion.on = 1;
    for (unsigned i = 0; i < MOTION_MAX; i++) {
        q->motion.event[i].place = i;
        const unsigned ids[] = {P_REV, P_GTYPE, P_GA, P_GB, P_GCUT, P_GRES, P_E0, P_E7};
        q->motion.event[i].param = ids[i % 8] | (i & 1 ? MOTION_LOCK : 0);
        q->motion.event[i].value = i % 128 - 64;
    }
    q->sum = proj_sum(q);
}
/* Frozen FUN10 geometry: raw 128-byte patches at 3312, MIDI at 3309.
 * Build from semantic fields, independently of the new writer's packed tail. */
static void fun10(project_store_t *s, const project_t *q, unsigned np)
{
    unsigned pos = 68, nc = np - 8; uint32_t magic = 0x46554E41, size = 3840;
    memset(s, 0, sizeof *s); memcpy(s->raw, &magic, 4); memcpy(s->raw + 4, &size, 4);
    memcpy(s->raw + 8, q->g, sizeof q->g); s->raw[62] = q->sel; s->raw[63] = q->parts;
    s->raw[64] = q->phys; s->raw[66] = np;
    for (unsigned t = 0; t < NTRK; t++) {
        for (unsigned i = 0; i < np; i++) s->raw[pos++] = q->t[t].p[i < nc ? i : P_E0 + i - nc] + 64;
        s->raw[pos++] = q->t[t].engine; s->raw[pos++] = q->t[t].preset;
        pos += NSTEP * 9; /* valid empty grid */
    }
    chain_config_t c = {0}; motion_store_t m = {0};
    memcpy(s->raw + pos, &c, sizeof c); pos += sizeof c;
    m.count = 1; m.on = 1; m.event[0].param = (np - 8) | MOTION_LOCK; m.event[0].value = 7;
    memcpy(s->raw + pos, &m, sizeof m);
    memcpy(s->raw + 3312, q->fm6, sizeof q->fm6);
    /* Legacy TRACK MIDI with explicit mixed channels. */
    uint32_t midi = 1u << 23;
    for (unsigned t = 0; t < NTRK; t++) midi |= (uint32_t)q->mch[t] << (t * 5);
    for (unsigned i = 0; i < 3; i++) s->raw[3309 + i] = midi >> (8*i);
    memcpy(s->raw + 3824, q->name, 12); seal(s);
}
int main(void)
{
    static project_t q, got; static project_store_t s, bad;
    REQUIRE(P_COUNT == 118 && sizeof s == 3840 && PROJ_FM6_STORE == 112);
    fixture(&q);
    REQUIRE(proj_pack(&s, &q)); REQUIRE(*(uint32_t *)s.raw == PROJ_MAGIC_V11); REQUIRE(proj_import(&got, &s, sizeof s));
    REQUIRE(!memcmp(&q, &got, sizeof q));
    for (unsigned t = 0; t < NTRK; t++) for (unsigned p = 0; p < P_COUNT; p++) REQUIRE(q.t[t].p[p] == got.t[t].p[p]);
    for (unsigned k = 5; k <= 12; k++) {
        for (unsigned t = 0; t < NTRK; t++) {
            q.t[t].engine = t & 1 ? ENGI_SAMPLE : ENGI_GRAIN; q.t[t].p[P_E0] = k;
        }
        q.sum = proj_sum(&q); REQUIRE(proj_pack(&s,&q) && proj_import(&got,&s,sizeof s));
        REQUIRE(!memcmp(&q,&got,sizeof q));
    }
    /* Every seven-bit value at every patch position, including cross-byte edges. */
    for (unsigned v = 0; v < 128; v++) {
        for (unsigned t = 0; t < NTRK; t++) for (unsigned i = 0; i < FM6_PACKED; i++) q.fm6[t][i] = (v + i + t) & 127;
        q.sum = proj_sum(&q); REQUIRE(proj_pack(&s,&q) && proj_import(&got,&s,sizeof s));
        REQUIRE(!memcmp(q.fm6,got.fm6,sizeof q.fm6));
    }
    for (unsigned i = 0; i < sizeof s; i++) for (unsigned bit = 0; bit < 8; bit++) {
        bad=s; bad.raw[i] ^= 1u << bit; REQUIRE(!proj_import(&got,&bad,sizeof bad));
    }
    for (unsigned n = 0; n < sizeof s; n++) REQUIRE(!proj_import(&got,&s,n));
    REQUIRE(!proj_unpack(&got,s.raw,0));
    q.fm6[3][127] = 128; REQUIRE(!proj_pack(&s,&q)); q.fm6[3][127] = 127;
    q.t[0].p[117] = 128; REQUIRE(!proj_pack(&s,&q)); q.t[0].p[117] = 0;
    REQUIRE(proj_pack(&s,&q));
    const unsigned offsets[] = {0,4,66,68,68+P_COUNT,68+P_COUNT+2+4,
        68+NTRK*(P_COUNT+2+NSTEP*9),68+NTRK*(P_COUNT+2+NSTEP*9)+84};
    for (unsigned i=0;i<sizeof offsets/sizeof *offsets;i++) {
        bad=s; bad.raw[offsets[i]]=255; seal(&bad); REQUIRE(!proj_import(&got,&bad,sizeof bad));
    }
    for (unsigned np=103;np<=114;np++) {
        fun10(&s,&q,np); REQUIRE(proj_import(&got,&s,sizeof s));
        for(unsigned t=0;t<NTRK;t++) {
            REQUIRE(!memcmp(q.fm6[t],got.fm6[t],128)); REQUIRE(got.mch[t]==q.mch[t]);
            for(unsigned p=0;p<np-8;p++) REQUIRE(got.t[t].p[p]==q.t[t].p[p]);
            for(unsigned p=0;p<8;p++) REQUIRE(got.t[t].p[P_E0+p]==q.t[t].p[P_E0+p]);
            for(unsigned p=np-8;p<P_E0;p++) REQUIRE(got.t[t].p[p]==param_desc_of(q.t[t].engine,p)->def);
        }
        REQUIRE(got.motion.event[0].param==(P_E0|MOTION_LOCK));
        REQUIRE(got.g[G_ROUTE]==ROUTE_TRACK && !memcmp(got.name,q.name,12));
        REQUIRE(proj_pack(&bad,&got) && proj_import(&q,&bad,sizeof bad));
        REQUIRE(!memcmp(&q,&got,sizeof q));
    }
    /* A former overlapping FUN10 is never interpreted as compact FM6. */
    fun10(&s,&q,118); REQUIRE(!proj_import(&got,&s,sizeof s));
    q.t[0].engine=ENGI_SAMPLE;q.t[0].p[P_E0]=4;
    fun10(&s,&q,111);REQUIRE(proj_import(&got,&s,sizeof s));REQUIRE(got.t[0].engine==ENGI_DRUM);
    REQUIRE(proj_pack(&s,&q)&&proj_import(&got,&s,sizeof s));REQUIRE(got.t[0].engine==ENGI_DRUM);
    printf("project format: %u checks passed (FUN11 + FUN10)\n",checks);
    return 0;
}
