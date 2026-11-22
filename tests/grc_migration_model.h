/* SPDX-License-Identifier: GPL-3.0-only */
/* TEST MODEL ONLY. Candidate identifiers below allocate no production format.
 * Standalone storage geometry, intentionally independent of P_COUNT/P_E0. */
#ifndef GRC_MIGRATION_MODEL_H
#define GRC_MIGRATION_MODEL_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
enum { MC_OLD_N=118, MC_NEW_N=119, MC_E0=110, MC_E7=117, MC_C=118,
 MC_TRACKS=4, MC_STEPS=64, MC_PROJECT=3840, MC_PRESET=192,
 MC_PARAMS_AT=68, MC_STEP_BYTES=576, MC_TAIL_BYTES=472,
 MC_MIDI_AT=3373, MC_FM_AT=3376, MC_NAME_AT=3824, MC_SUM_AT=3836,
 MC_GRAIN=8, MC_ENGINE_N=14, MC_NORMAL_CANDIDATE=8, MC_GRID_CANDIDATE=9 };
#define MC_FUNC UINT32_C(0x46554e43)
#define MC_FUND_CANDIDATE UINT32_C(0x46554e44)
typedef union { uint32_t align[960]; uint8_t raw[MC_PROJECT]; } mc_project;
typedef union { uint32_t align[48]; uint8_t raw[MC_PRESET]; } mc_preset;
typedef struct { int16_t p[118]; uint8_t engine,preset,steps[64][11]; } mc_track118;
typedef struct { int16_t p[119]; uint8_t engine,preset,steps[64][11]; } mc_track119;
#define MC_RUNTIME(T) struct { uint32_t magic,size; int16_t globals[27]; uint8_t sel,parts,phys,rsv; T track[4]; uint8_t chain[84],motion[388],fm6[4][128],mch[4],name[12]; uint32_t sum; }
typedef MC_RUNTIME(mc_track118) mc_runtime118;
typedef MC_RUNTIME(mc_track119) mc_runtime119;
#undef MC_RUNTIME
_Static_assert(sizeof(mc_runtime118)==4840 && sizeof(mc_runtime119)==4848,"frozen/runtime model sizes");
_Static_assert(sizeof(mc_project)==3840 && sizeof(mc_preset)==192,"fixed storage model");
static uint32_t mc_r32(const uint8_t*p){return(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void mc_w32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t mc_hash(const void*raw,size_t n){const uint8_t*p=raw;uint32_t h=2166136261u;for(size_t i=0;i<n;i++)h=(h^p[i])*16777619u;return h;}
static void mc_seal(mc_project*p){mc_w32(p->raw+MC_SUM_AT,mc_hash(p->raw,MC_SUM_AT));}
/* Legacy count is NOT a layout marker: even np119 can be legacy tail-eight. */
static int mc_historical(int16_t out[119],const int16_t*in,size_t available,unsigned np,const int16_t def[118]){
 if(np<8||np>144||available<np)return 0;int16_t stage[119];unsigned common=np-8;
 for(unsigned i=0;i<110;i++)stage[i]=i<common?in[i]:def[i];
 for(unsigned i=0;i<8;i++)stage[110+i]=in[common+i];stage[118]=0;
 memcpy(out,stage,sizeof stage);return 1;
}
static int mc_stable(int16_t out[119],const int16_t*in,size_t available,unsigned np,unsigned engine){
 if(engine>=MC_ENGINE_N||np!=119||available<119||in[118]<0||in[118]>100||(engine!=8&&in[118]))return 0;
 for(unsigned i=0;i<118;i++)if(in[i]<-64||in[i]>127)return 0;
 if(engine==8?(in[109]<0||in[109]>127):in[109]!=0)return 0;
 memcpy(out,in,119*sizeof *out);return 1;
}
/* Frozen legacy constants, NOT new_count - np. Preserve lock flag (bit7). */
static int mc_motion(uint8_t*dst,unsigned raw,unsigned np,int appended){
 unsigned id=raw&127u,result;
 if(raw>255||np<8||np>119)return 0;
 if(appended){if(np!=119||id>=118)return 0;result=id;}
 else{if(np>118||id>=np)return 0;result=np<118&&id>=np-8?id+118-np:id;}
 *dst=(uint8_t)((raw&128u)|result);return 1;
}
typedef int(*mc_validate)(const uint8_t*,size_t);
/* Model only the new mapping; delegate unchanged opaque fields to the frozen
 * production reader. Output is staged until full validation succeeds. */
static int mc_project_pack(mc_project*out,const mc_project*old,const int16_t c[4],mc_validate validate){
 if(!validate||mc_r32(old->raw)!=MC_FUNC||old->raw[66]!=118||!validate(old->raw,3840))return 0;
 mc_project p=*old;mc_w32(p.raw,MC_FUND_CANDIDATE);p.raw[66]=119;
 for(unsigned t=0;t<4;t++){
  unsigned src=68+t*(118+2+576),at=68+t*(119+2+576),engine=old->raw[src+118];
  if(c[t]<0||c[t]>100||(engine!=8&&c[t]))return 0;
  memcpy(p.raw+at,old->raw+src,118);p.raw[at+118]=(uint8_t)(c[t]+64);
  memcpy(p.raw+at+119,old->raw+src+118,2+576);
 }
 memcpy(p.raw+68+4*(119+2+576),old->raw+68+4*(118+2+576),472);
 mc_seal(&p);*out=p;return 1;
}
static int mc_project_unpack(mc_project*out,int16_t c[4],const void*raw,size_t n,mc_validate validate){
 if(!raw||n!=3840||!validate)return 0;const uint8_t*p=raw;
 if(mc_r32(p)!=MC_FUND_CANDIDATE||mc_r32(p+4)!=3840||p[66]!=119||mc_hash(p,3836)!=mc_r32(p+3836))return 0;
 mc_project old;memcpy(old.raw,p,3840);int16_t values[4];mc_w32(old.raw,MC_FUNC);old.raw[66]=118;
 for(unsigned t=0;t<4;t++){
  unsigned src=68+t*(119+2+576),at=68+t*(118+2+576);
  if(p[src+118]<64||p[src+118]>164)return 0;values[t]=p[src+118]-64;
  if(p[src+119]!=8&&values[t])return 0;
  memcpy(old.raw+at,p+src,118);memcpy(old.raw+at+118,p+src+119,2+576);
 }
 memcpy(old.raw+68+4*(118+2+576),p+68+4*(119+2+576),472);
 /* Candidate has four fewer spare bytes. Normalize scratch only; never raw input. */
 memset(old.raw+68+4*(118+2+576)+472,0,49);
 mc_seal(&old);if(!validate(old.raw,3840))return 0;*out=old;memcpy(c,values,sizeof values);return 1;
}
static int mc_preset_pack(mc_preset*out,const mc_preset*old,int c,mc_validate validate){
 if(!validate||!validate(old->raw,192)||old->raw[1]<6||old->raw[1]>7||old->raw[3]!=118||c<0||c>100||(old->raw[2]!=8&&c))return 0;
 mc_preset p=*old;p.raw[1]=(uint8_t)(old->raw[1]+2);p.raw[3]=119;p.raw[16+118]=(uint8_t)(c+64);*out=p;return 1;
}
static int mc_preset_unpack(mc_preset*out,int*c,const void*raw,size_t n,mc_validate validate){
 if(!raw||n!=192||!validate)return 0;const uint8_t*p=raw;
 if((p[1]!=8&&p[1]!=9)||p[3]!=119||p[16+118]<64||p[16+118]>164||(p[2]!=8&&p[16+118]!=64))return 0;
 mc_preset old;memcpy(old.raw,p,192);old.raw[1]-=2;old.raw[3]=118;
 if(!validate(old.raw,192))return 0;*out=old;*c=p[16+118]-64;return 1;
}
static int mc_runtime_upgrade(mc_runtime119*out,const void*raw,size_t n){
 if(!raw||n!=4840)return 0;mc_runtime118 old;memcpy(&old,raw,n);
 if(old.magic!=MC_FUNC||old.size!=4840||mc_hash(&old,4836)!=old.sum)return 0;
 mc_runtime119 next;memset(&next,0,sizeof next);next.magic=MC_FUND_CANDIDATE;next.size=4848;
 memcpy(next.globals,old.globals,sizeof old.globals);memcpy(&next.sel,&old.sel,4);
 for(unsigned t=0;t<4;t++){memcpy(next.track[t].p,old.track[t].p,236);next.track[t].p[118]=0;next.track[t].engine=old.track[t].engine;next.track[t].preset=old.track[t].preset;memcpy(next.track[t].steps,old.track[t].steps,704);}
 memcpy(next.chain,old.chain,84);memcpy(next.motion,old.motion,388);memcpy(next.fm6,old.fm6,512);memcpy(next.mch,old.mch,4);memcpy(next.name,old.name,12);
 next.sum=mc_hash(&next,4844);*out=next;return 1;
}
static int mc_runtime_read(mc_runtime119*out,const void*raw,size_t n,mc_validate validate){
 if(!raw||n!=4848||!validate)return 0;mc_runtime119 next;memcpy(&next,raw,n);
 if(next.magic!=MC_FUND_CANDIDATE||next.size!=4848||mc_hash(&next,4844)!=next.sum)return 0;
 for(unsigned t=0;t<4;t++){
  if(next.track[t].engine>=MC_ENGINE_N||next.track[t].p[118]<0||next.track[t].p[118]>100||(next.track[t].engine!=8&&next.track[t].p[118]))return 0;
 }
 mc_runtime118 old;memset(&old,0,sizeof old);old.magic=MC_FUNC;old.size=4840;
 memcpy(old.globals,next.globals,sizeof old.globals);memcpy(&old.sel,&next.sel,4);
 for(unsigned t=0;t<4;t++){int16_t checked[119];if(!mc_stable(checked,next.track[t].p,119,119,next.track[t].engine))return 0;
  memcpy(old.track[t].p,next.track[t].p,236);old.track[t].engine=next.track[t].engine;old.track[t].preset=next.track[t].preset;memcpy(old.track[t].steps,next.track[t].steps,704);}
 memcpy(old.chain,next.chain,84);memcpy(old.motion,next.motion,388);memcpy(old.fm6,next.fm6,512);memcpy(old.mch,next.mch,4);memcpy(old.name,next.name,12);old.sum=mc_hash(&old,4836);
 if(!validate((const uint8_t*)&old,4840))return 0;*out=next;return 1;
}
/* Abstract provenance policy; these flags allocate NO wire tags or commands.
 * WIN-aware library activation is not assumed. Existing non-Hann restrictions
 * are preserved; only explicit scalar edits may change WIN. */
static int mc_bulk(int16_t out[119],const int16_t*in,size_t n,unsigned engine,int marked,int capable,const int16_t current[119]){
 if(engine==8&&current[109]!=64)return 0;
 if(n>144||engine>=MC_ENGINE_N||(marked!=0&&marked!=1))return 0;
 int16_t stage[119],def[118]={0};
 if(!marked){if(current[118]||(engine==8&&current[109]!=64))return 0;if(!mc_historical(stage,in,n,(unsigned)n,def))return 0;stage[109]=engine==8?64:0;}
 else{if(capable!=1||!mc_stable(stage,in,n,(unsigned)n,engine)||(engine==8&&stage[109]!=64))return 0;}
 memcpy(out,stage,sizeof stage);return 1;
}
static int mc_scalar(int16_t values[4][119],unsigned track,unsigned id,int value,unsigned engine,int capable){
 if(capable!=1||engine>=MC_ENGINE_N||track>=4||id!=118||value<0||value>100||(engine!=8&&value))return 0;values[track][118]=(int16_t)value;return 1;
}
#endif
