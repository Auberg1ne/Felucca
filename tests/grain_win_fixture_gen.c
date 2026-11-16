/* SPDX-License-Identifier: GPL-3.0-only */
/* Freeze with the exact B1 source before modifying converters. Never silently
 * regenerate these inputs with a WIN-aware writer. */
#define UI_TEST_NO_MAIN 1
#ifndef WIN_BASE_UI_SOURCE
#error "Choose the exact B1 tests/ui_test.c explicitly"
#endif
#include WIN_BASE_UI_SOURCE
#include "old_pack.h"
#include <assert.h>
static const unsigned values[]={0,1,63,64,65,126,127};
static void save(const char*name,const void*p,unsigned n){char path[256];snprintf(path,sizeof path,"tests/fixtures/grain-win-i1r/%s.bin",name);FILE*f=fopen(path,"wb");assert(f && fwrite(p,1,n,f)==n);fclose(f);project_t got;assert(proj_import(&got,p,n));}
static void seal(void*p,unsigned n){uint32_t h=proj_hash(p,n-4);memcpy((uint8_t*)p+n-4,&h,4);}
static void fun10(project_store_t*s,const project_t*q){
 unsigned pos=68,np=114,nc=np-8;uint32_t magic=PROJ_MAGIC_V10,size=3840;memset(s,0,sizeof*s);memcpy(s->raw,&magic,4);memcpy(s->raw+4,&size,4);memcpy(s->raw+8,q->g,sizeof q->g);s->raw[63]=NPART;s->raw[64]=PROJ_PHYS;s->raw[66]=np;
 for(unsigned t=0;t<NTRK;t++){for(unsigned i=0;i<np;i++)s->raw[pos++]=q->t[t].p[i<nc?i:P_E0+i-nc]+64;s->raw[pos++]=ENGI_GRAIN;s->raw[pos++]=0;pos+=NSTEP*9;}
 memcpy(s->raw+pos,&q->chain,sizeof q->chain);pos+=sizeof q->chain;memcpy(s->raw+pos,&q->motion,sizeof q->motion);memcpy(s->raw+3312,q->fm6,sizeof q->fm6);seal(s,sizeof*s);
}
int main(void){
 ui_power_on();project_t q;project_capture(&q);memset(&q.chain,0,sizeof q.chain);memset(&q.motion,0,sizeof q.motion);q.sum=proj_sum(&q);
 project_v1_t v1={0};v1.magic=PROJ_MAGIC_V1;v1.size=sizeof v1;v1.t.engine=ENGI_GRAIN;seal(&v1,sizeof v1);save("v1",&v1,sizeof v1);
 project_v2_t v2={0};v2.magic=PROJ_MAGIC_V2;v2.size=sizeof v2;for(unsigned t=0;t<NTRK;t++)v2.t[t].engine=ENGI_GRAIN;seal(&v2,sizeof v2);save("v2",&v2,sizeof v2);
 project_v3_t v3={0};v3.magic=PROJ_MAGIC_V3;v3.size=sizeof v3;v3.parts=NPART;for(unsigned t=0;t<NTRK;t++)v3.t[t].engine=ENGI_GRAIN;seal(&v3,sizeof v3);save("v3",&v3,sizeof v3);
 project_v4_t v4={0};v4.magic=PROJ_MAGIC_V4;v4.size=sizeof v4;v4.parts=NPART;v4.phys=PROJ_PHYS;for(unsigned t=0;t<NTRK;t++)v4.t[t].engine=ENGI_GRAIN;seal(&v4,sizeof v4);save("v4",&v4,sizeof v4);
 project_v5_t v5={0};v5.magic=PROJ_MAGIC_V5;v5.size=sizeof v5;v5.parts=NPART;v5.phys=PROJ_PHYS;for(unsigned t=0;t<NTRK;t++)v5.t[t].engine=ENGI_GRAIN;seal(&v5,sizeof v5);save("v5",&v5,sizeof v5);
 project_v6_t v6={0};v6.magic=PROJ_MAGIC_V6;v6.size=sizeof v6;v6.parts=NPART;v6.phys=PROJ_PHYS;for(unsigned t=0;t<NTRK;t++)v6.t[t].engine=ENGI_GRAIN;seal(&v6,sizeof v6);save("v6",&v6,sizeof v6);
 uint8_t raw[3840];unsigned n=old_pack(raw,&q,OLD_FUN7,91);assert(n);save("v7",raw,n);n=old_pack(raw,&q,OLD_FUN8,91);assert(n);save("v8",raw,n);n=old_pack(raw,&q,OLD_FUN9,99);assert(n);save("v9",raw,n);
 project_store_t st;fun10(&st,&q);save("v10",&st,sizeof st);
 for(unsigned i=0;i<NELEM(values);i++){char name[64];for(unsigned t=0;t<NTRK;t++)q.t[t].p[P_GFILTER_RSVD2]=values[i];q.sum=proj_sum(&q);assert(proj_pack(&st,&q));snprintf(name,sizeof name,"v11-%u",values[i]);save(name,&st,sizeof st);snprintf(name,sizeof name,"retained-%u",values[i]);save(name,&q,sizeof q);}
 for(unsigned ver=1;ver<=5;ver++)for(unsigned i=0;i<(ver>=4?NELEM(values):1u);i++){
  up_rec_t r={0};r.used=UP_USED;r.ver=ver;r.engine=ENGI_GRAIN;r.np=ver>=4?118:53;memcpy(r.name,"LEGACY",6);for(unsigned k=0;k<r.np;k++)up_set_value(&r,k,0);if(ver>=4)up_set_value(&r,P_GFILTER_RSVD2,values[i]);assert(up_valid(&r));char name[64];snprintf(name,sizeof name,"up-v%u-%u",ver,ver>=4?values[i]:0);char path[256];snprintf(path,sizeof path,"tests/fixtures/grain-win-i1r/%s.bin",name);FILE*f=fopen(path,"wb");assert(f && fwrite(&r,1,sizeof r,f)==sizeof r);fclose(f);
 }
 puts("B1 fixtures frozen: versions1..11 and legacy retained schema; all accepted by exact B1 reader");return 0;
}
