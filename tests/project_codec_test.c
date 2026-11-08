/* SPDX-License-Identifier: GPL-3.0-only */
/* Actual native project/storage/backup handlers, on RAM NOR. */
#define BACKUP_TEST_NO_MAIN 1
#include "backup_test.c"
static project_t q, decoded;
static project_store_t wire, oldwire;
static int failures, passed;
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 0;}}while(0)
static void run_test(const char *name,int (*fn)(void)) {int ok=fn();printf("codec: %s %s\n",name,ok?"ok":"FAIL");if(ok)passed++;else failures++;}
static void fixture(void)
{
    reset();host_preset(&trk[0],ENGI_GRAIN,0);trk[0].p[P_E0]=12;
    project_capture(&q);q.parts=NPART;q.phys=PROJ_PHYS;
    chain_defaults(&q.chain);memset(&q.motion,0,sizeof q.motion);
}
static void rehash(uint8_t *b,uint32_t n){uint32_t h=proj_hash(b,n-4);memcpy(b+n-4,&h,4);}
static int proof(void)
{
    fixture();uint32_t checks=0;
    for(uint32_t v=7;v<=10;v++) {
        uint32_t np=v==7?69:v==8?91:v==9?99:111;
        uint32_t n=v==10?old_pack10(oldwire.raw,&q,np):old_pack(oldwire.raw,&q,v==7?OLD_FUN7:v==8?OLD_FUN8:OLD_FUN9,np);
        REQUIRE(n && proj_import(&decoded,oldwire.raw,n));
        for(uint32_t t=0;t<NTRK;t++)for(uint32_t endpoint=0;endpoint<2;endpoint++) {
            uint32_t at=68+t*(np+2+NSTEP*9)+np+2+(endpoint?63:0)*9+4;uint8_t keep=oldwire.raw[at];
            for(uint32_t meta=0;meta<256;meta++) {
                oldwire.raw[at]=(uint8_t)meta;rehash(oldwire.raw,n);
                int expected=!(meta&128u)&&(meta&7u)<=4&&((meta>>3)&3u)<=ST_REST;
                REQUIRE(proj_import(&decoded,oldwire.raw,n)==expected);checks++;
            }
            oldwire.raw[at]=keep;rehash(oldwire.raw,n);
        }
    }
    /* Old raw fields are not a packed metadata byte. Existing conversion and
     * runtime bounds remain unchanged; invalid runtime n/time are never masked
     * into the new bitstream by the writer. */
    project_v6_t raw;memset(&raw,0,sizeof raw);raw.magic=PROJ_MAGIC_V6;raw.size=sizeof raw;raw.parts=NPART;raw.phys=PROJ_PHYS;
    for(uint32_t t=0;t<NTRK;t++){raw.t[t].engine=ENGI_GRAIN;raw.t[t].step[63].n=255;raw.t[t].step[63].time=16;raw.t[t].step[63].flags=255;}
    raw.sum=proj_hash(&raw,sizeof raw-4);REQUIRE(proj_import(&decoded,&raw,sizeof raw));
    REQUIRE(decoded.t[0].step[63].n==255&&decoded.t[0].step[63].time==16);
    proj_bound(&decoded);REQUIRE(decoded.t[0].step[63].n==4&&decoded.t[0].step[63].time==ST_REST);
    printf("metadata acceptance checks=%u; legacy raw bounds unchanged\n",checks);return 1;
}
static void rich(void)
{
    fixture();q.g[G_ROUTE]=ROUTE_TRACK;
    for(uint32_t t=0;t<NTRK;t++) {
        q.t[t].engine=ENGI_GRAIN;q.t[t].p[P_E0]=(int16_t)(t==3?12:t);
        q.t[t].p[P_GTYPE]=t;q.t[t].p[P_GA]=127-t;q.t[t].p[P_GB]=17+t;
        q.t[t].p[P_GCUT]=-50+(int16_t)t;q.t[t].p[P_GRES]=120+t;
        q.mch[t]=(uint8_t)(t==0?17:t==1?16:t==2?15:0);
        memcpy(q.fm6[t],FM6_FACTORY[t*2],FM6_PACKED);
        for(uint32_t i=0;i<NSTEP;i++) {
            step_t *x=&q.t[t].step[i];uint32_t k=t*NSTEP+i;
            for(uint32_t j=0;j<4;j++)x->note[j]=(uint8_t)((k*17+j*33)&127);
            x->n=k%5;x->time=k%3;x->flags=(uint8_t)k;x->vel=(uint8_t)(k&127);
            x->hit=(uint8_t)(k*13);x->acc=(uint8_t)((k*7)&x->hit);x->probability=k%(COND_FIRST+NCOND);
        }
    }
    memset(&q.chain,0xFF,sizeof q.chain);q.chain.count=0;
    q.motion.count=MOTION_MAX;q.motion.on=15;q.motion.rsv[0]=0xAB;q.motion.rsv[1]=0xCD;
    for(uint32_t i=0;i<MOTION_MAX;i++)q.motion.event[i]=(motion_event_t){i,(uint8_t)(P_E0|(i&1?MOTION_LOCK:0)),i%13};
    q.motion.event[0].param=P_GCUT|MOTION_LOCK;q.motion.event[0].value=-50;
    q.motion.event[1].param=P_GTYPE;q.motion.event[1].value=3;
    q.motion.event[2].param=P_GA;q.motion.event[2].value=127;
    q.motion.event[3].param=P_GB;q.motion.event[3].value=127;
    q.motion.event[4].param=P_GRES;q.motion.event[4].value=127;
    memset(q.name,0,sizeof q.name);memcpy(q.name,"CODEC USR8",10);q.sum=proj_sum(&q);
}
static int roundtrip(void)
{
    rich();REQUIRE(PROJ_MAGIC==0x46554E42u&&PROJ_MAGIC_V10==OLD_FUN10);
    REQUIRE(PROJ_TRACK_STEPS==568&&PROJ_STORE_SIZE==3840);
    REQUIRE(proj_pack(&wire,&q)&&proj_import(&decoded,&wire,sizeof wire));
    REQUIRE(!memcmp(&q,&decoded,sizeof q));
    uint32_t end=68+NTRK*(P_COUNT+2+PROJ_TRACK_STEPS)+sizeof q.chain+sizeof q.motion;
    REQUIRE(end==3292&&PROJ_MIDI_OFF-end==17);
    for(uint32_t i=end;i<PROJ_MIDI_OFF;i++)REQUIRE(!wire.raw[i]);
    for(uint32_t t=0;t<NTRK;t++)for(uint32_t i=0;i<NSTEP;i++)REQUIRE(!memcmp(&q.t[t].step[i],&decoded.t[t].step[i],sizeof(step_t)));
    return 1;
}
static int bit_basis(void)
{
    { /* Independent hand-computed LSB-first format vector. */
      const uint8_t input[9]={1,2,4,8,16,32,64,128,1},gold[9]={1,2,4,8,16,16,32,192,0};
      uint8_t bytes[PROJ_TRACK_STEPS]={0};uint32_t bit=0;
      REQUIRE(proj_step_put71(bytes,&bit,input)&&bit==71&&!memcmp(bytes,gold,9));
    }
    uint8_t guarded[PROJ_TRACK_STEPS+2],r[9],out[9];uint32_t checked=0;
    for(uint32_t slot=0;slot<NSTEP;slot++)for(uint32_t b=0;b<71;b++) {
        memset(guarded,0,sizeof guarded);guarded[0]=0xA5;guarded[sizeof guarded-1]=0x5A;memset(r,0,9);
        uint32_t j=b<32?b/8:b<39?4:5+(b-39)/8,k=b<32?b%8:b<39?b-32:(b-39)%8;
        r[j]=(uint8_t)(1u<<k);uint32_t bit=slot*71;
        REQUIRE(proj_step_put71(guarded+1,&bit,r)&&bit==(slot+1)*71);
        bit=slot*71;REQUIRE(proj_step_get71(guarded+1,&bit,out)&&!memcmp(r,out,9));
        REQUIRE(guarded[0]==0xA5&&guarded[sizeof guarded-1]==0x5A);checked++;
    }
    uint32_t bit=PROJ_TRACK_STEPS*8;REQUIRE(!proj_step_put71(guarded+1,&bit,r));
    REQUIRE(!proj_step_get71(guarded+1,&bit,out));
    memset(r,0,9);r[4]=128;bit=0;REQUIRE(!proj_step_put71(guarded+1,&bit,r));
    printf("71-bit position/basis cases=%u\n",checked);return 1;
}
static int all_sources(void)
{
    for(uint32_t e=4;e<=8;e+=4)for(uint32_t src=0;src<13;src++) {
        fixture();host_preset(&trk[0],e,0);trk[0].p[P_E0]=src;
        REQUIRE(project_save_as(0,"ALL SOURCES")==0);
        REQUIRE(((uint32_t *)proj_slot[0].raw)[0]==PROJ_MAGIC);
        trk[0].p[P_E0]=0;memset(proj_slot,0,sizeof proj_slot);proj_fetch(0);
        REQUIRE(project_used(0));project_load(0);
        REQUIRE(trk[0].eng_req==e&&trk[0].p[P_E0]==src);
    }return 1;
}
static int active_chain(void)
{
    fixture();q.chain.count=16;q.chain.rsv[0]=0x12;q.chain.rsv[1]=0x34;q.chain.rsv[2]=0x56;
    for(uint32_t i=0;i<16;i++) {for(uint32_t t=0;t<4;t++)q.chain.row[i].slot[t]=(i+t)%5;q.chain.row[i].repeat=i+1;}
    REQUIRE(proj_pack(&wire,&q)&&proj_import(&decoded,&wire,sizeof wire));
    REQUIRE(!memcmp(&q.chain,&decoded.chain,84));return 1;
}
static int save_backup(void)
{
    rich();REQUIRE(project_restore_runtime(&q)==0);
    REQUIRE(project_save_as(0,"CODEC USR8")==0);REQUIRE(project_used(0));
    REQUIRE(proj_import(&decoded,&proj_slot[0],sizeof wire));
    REQUIRE(decoded.t[0].p[P_GCUT]==-50&&decoded.t[3].p[P_E0]==12);
    REQUIRE(!memcmp(&q.chain,&decoded.chain,84)&&!memcmp(&q.motion,&decoded.motion,sizeof q.motion));
    REQUIRE(!memcmp(q.mch,decoded.mch,4)&&!memcmp(q.fm6,decoded.fm6,sizeof q.fm6));
    uint32_t n=0,c=0;REQUIRE(list(0,&n,&c)==0&&n==3840&&c==st_crc32(ED_BK_RAW,n));
    REQUIRE(((uint32_t *)ED_BK_RAW)[0]==PROJ_MAGIC);memcpy(&wire,ED_BK_RAW,sizeof wire);
    REQUIRE(put_all(3,&wire,sizeof wire,st_crc32(&wire,sizeof wire))==0);
    REQUIRE(project_used(1));REQUIRE(proj_import(&decoded,&proj_slot[1],sizeof wire));
    REQUIRE(decoded.t[3].p[P_E0]==12&&!memcmp(&q.chain,&decoded.chain,84));
    REQUIRE(put_all(0,&wire,sizeof wire,st_crc32(&wire,sizeof wire))==0);
    REQUIRE(trk[3].p[P_E0]==12&&trk[0].p[P_GCUT]==-50);
    for(uint32_t i=0;i<10;i++){trk[0].p[P_GCUT]=-50+(int16_t)i;REQUIRE(project_save(0)==0);memset(proj_slot,0,sizeof proj_slot);proj_fetch(0);project_load(0);REQUIRE(trk[0].p[P_GCUT]==-50+(int16_t)i);}
    return 1;
}
static int legacy(void)
{
    fixture();memset(&q.chain,0xA5,84);q.chain.count=0;
    for(uint32_t v=7;v<=10;v++) {
        uint32_t np=v==7?69:v==8?91:v==9?99:111,n=v==10?old_pack10(oldwire.raw,&q,np):old_pack(oldwire.raw,&q,v==7?OLD_FUN7:v==8?OLD_FUN8:OLD_FUN9,np);
        REQUIRE(n&&proj_import(&decoded,oldwire.raw,n)&&decoded.t[0].p[P_E0]==12);
        REQUIRE(st_save(OBJ_PROJECT0,oldwire.raw,n)==0);memset(proj_slot,0,sizeof proj_slot);proj_fetch(0);
        REQUIRE(!memcmp(proj_slot[0].raw,oldwire.raw,n));REQUIRE(project_used(0));
        project_load(0);REQUIRE(trk[0].p[P_E0]==12);
        if(v==10)REQUIRE(!memcmp(&chain_config,&q.chain,84));
        REQUIRE(put_all(3,&oldwire,n,st_crc32(&oldwire,n))==0);
        REQUIRE(((uint32_t *)proj_slot[1].raw)[0]==PROJ_MAGIC);
    }
    /* Legacy SAMPLE 4 still migrates, while FUNB 4 is a canonical alias. */
    q.t[0].engine=ENGI_SAMPLE;q.t[0].p[P_E0]=4;
    REQUIRE(old_pack10(oldwire.raw,&q,111)&&proj_import(&decoded,&oldwire,3840)&&decoded.t[0].engine==ENGI_DRUM);
    return 1;
}
static int legacy_raw(void)
{
    static union {project_v1_t v1;project_v2_t v2;project_v3_t v3;project_v4_t v4;project_v5_t v5;project_v6_t v6;} u;
    for(uint32_t v=1;v<=6;v++) {
        fixture();memset(&u,0,sizeof u);uint32_t n=0,magic=PROJ_MAGIC_V1+v-1;
        if(v==1){n=sizeof u.v1;u.v1.t.engine=ENGI_SAMPLE;u.v1.t.p[45]=5;}
        if(v==2){n=sizeof u.v2;for(uint32_t t=0;t<NTRK;t++){u.v2.t[t].engine=ENGI_SAMPLE;u.v2.t[t].p[45]=5;}}
        if(v==3){n=sizeof u.v3;u.v3.parts=NPART;for(uint32_t t=0;t<NTRK;t++){u.v3.t[t].engine=ENGI_SAMPLE;u.v3.t[t].p[49]=5;}}
        if(v==4){n=sizeof u.v4;u.v4.parts=NPART;u.v4.phys=PROJ_PHYS;for(uint32_t t=0;t<NTRK;t++){u.v4.t[t].engine=ENGI_SAMPLE;u.v4.t[t].p[61]=5;}}
        if(v==5){n=sizeof u.v5;u.v5.parts=NPART;u.v5.phys=PROJ_PHYS;for(uint32_t t=0;t<NTRK;t++){u.v5.t[t].engine=ENGI_SAMPLE;u.v5.t[t].p[61]=5;}}
        if(v==6){n=sizeof u.v6;u.v6.parts=NPART;u.v6.phys=PROJ_PHYS;for(uint32_t t=0;t<NTRK;t++){u.v6.t[t].engine=ENGI_SAMPLE;u.v6.t[t].p[61]=5;}}
        memcpy(&u,&magic,4);memcpy((uint8_t *)&u+4,&n,4);rehash((uint8_t *)&u,n);
        REQUIRE(proj_import(&decoded,&u,n)&&decoded.t[0].p[P_E0]==5);
        REQUIRE(st_save(OBJ_PROJECT0,&u,n)==0);memset(proj_slot,0,sizeof proj_slot);proj_fetch(0);
        REQUIRE(!memcmp(proj_slot[0].raw,&u,n));uint32_t len=0,crc=0;
        REQUIRE(list(2,&len,&crc)==0&&len==n&&crc==st_crc32(&u,n));
        REQUIRE(put_all(3,&u,n,st_crc32(&u,n))==0&&project_used(1));
        REQUIRE(proj_import(&decoded,&proj_slot[1],sizeof wire)&&decoded.t[0].p[P_E0]==5);
    }
    fixture();q.magic=PROJ_MAGIC_V10;q.sum=proj_sum(&q);
    REQUIRE(proj_import(&decoded,&q,sizeof q)&&decoded.magic==PROJ_MAGIC&&decoded.t[0].p[P_E0]==12);
    return 1;
}
static int maximum(void)
{
    rich();q.chain.count=16;
    for(uint32_t i=0;i<16;i++)q.chain.row[i]=(chain_row_t){{4,4,4,4},16};
    for(uint32_t t=0;t<NTRK;t++) {
        for(uint32_t p=0;p<P_COUNT;p++)q.t[t].p[p]=param_desc_of(ENGI_GRAIN,p)->max;
        for(uint32_t i=0;i<NSTEP;i++)q.t[t].step[i]=(step_t){{127,127,127,127},4,ST_REST,255,127,255,255,COND_FIRST+NCOND-1};
    }
    q.sum=proj_sum(&q);
    static struct {uint32_t before;project_store_t frame;uint32_t after;} canary;
    canary.before=0x11223344;canary.after=0x55667788;
    REQUIRE(proj_pack(&canary.frame,&q)&&canary.before==0x11223344&&canary.after==0x55667788);
    REQUIRE(proj_import(&decoded,&canary.frame,3840)&&!memcmp(&q,&decoded,sizeof q));
    uint32_t er=erases;REQUIRE(st_save(OBJ_PROJECT0,&canary.frame,3841)!=0&&erases==er);
    q.t[0].p[P_GA]=128;REQUIRE(!proj_pack(&wire,&q));q.t[0].p[P_GA]=-65;REQUIRE(!proj_pack(&wire,&q));
    return 1;
}
static int malformed(void)
{
    rich();REQUIRE(proj_pack(&wire,&q));static project_store_t bad;
    REQUIRE(project_restore_runtime(&q)==0 && st_save(OBJ_PROJECT0,&wire,3840)==0);
    proj_fetch(0);REQUIRE(project_used(0));static project_store_t cached_before;cached_before=proj_slot[0];
    uint32_t nor_crc=st_crc32(nor,sizeof nor);static project_t live_before,live_after;project_capture(&live_before);
    for(uint32_t n=0;n<sizeof wire;n++)REQUIRE(!proj_import(&decoded,&wire,n));
    for(uint32_t i=0;i<sizeof wire;i++){bad=wire;bad.raw[i]^=1;REQUIRE(!proj_import(&decoded,&bad,sizeof bad));}
    for(uint32_t field=0;field<4;field++) {
        bad=wire;
        if(field==0)bad.raw[PROJ_FM6_OFF]|=128u;
        if(field==1)bad.raw[PROJ_MIDI_OFF]=(bad.raw[PROJ_MIDI_OFF]&~31u)|31u;
        if(field==2)bad.raw[PROJ_MIDI_OFF+2]|=0x10u;
        if(field==3)bad.raw[PROJ_NAME_OFF]=7;
        rehash(bad.raw,3840);REQUIRE(!proj_import(&decoded,&bad,3840));
        uint32_t er=erases;REQUIRE(put_all(2,&bad,3840,st_crc32(&bad,3840))!=0&&erases==er);
    }
    bad=wire;uint32_t unknown=0x46554E43u;memcpy(bad.raw,&unknown,4);rehash(bad.raw,3840);REQUIRE(!proj_import(&decoded,&bad,3840));
    bad=wire;bad.raw[66]=P_COUNT+1;rehash(bad.raw,3840);REQUIRE(!proj_import(&decoded,&bad,3840));
    bad=wire;uint32_t bit=0;uint8_t raw[9]={0};raw[4]=7;
    uint32_t at=68+P_COUNT+2;memset(bad.raw+at,0,PROJ_TRACK_STEPS);REQUIRE(proj_step_put71(bad.raw+at,&bit,raw));rehash(bad.raw,3840);
    REQUIRE(!proj_import(&decoded,&bad,3840));
    uint32_t before=erases;REQUIRE(put_all(2,&bad,sizeof bad,st_crc32(&bad,sizeof bad))!=0&&erases==before);
    project_capture(&live_after);
    REQUIRE(!memcmp(&live_before,&live_after,sizeof live_before));
    REQUIRE(!memcmp(&cached_before,&proj_slot[0],sizeof cached_before));
    REQUIRE(st_crc32(nor,sizeof nor)==nor_crc);
    q.t[0].step[63].n=128;REQUIRE(!proj_pack(&bad,&q));q.t[0].step[63].n=0;q.t[0].step[63].time=16;REQUIRE(!proj_pack(&bad,&q));
    return 1;
}
static int32_t budget=-1,erase_cut=-1;static int ack_loss,readback_loss,armed,bad_write;
static uint32_t limit_lo,limit_hi;
static int fault_read(uint32_t off,void *dst,uint32_t n){if(armed&&off>=limit_lo&&off<limit_hi){armed=0;return -9;}return bk_read_plain(off,dst,n);}
static int fault_erase(uint32_t off){if(off<limit_lo||off+4096>limit_hi){bad_write=1;return -9;}if(erase_cut>=0){memset(nor+off,255,(uint32_t)erase_cut);return -9;}return bk_erase_plain(off);}
static int fault_prog(uint32_t off,const void *src,uint32_t n)
{
    if(off<limit_lo||off>=limit_hi||n>limit_hi-off){bad_write=1;return -9;}
    for(uint32_t i=0;i<n;i++){if(!budget)return -9;if(budget>0)budget--;nor[off+i]&=((const uint8_t *)src)[i];}
    if((off&4095u)==0&&n==32){if(readback_loss)armed=1;if(ack_loss)return -9;}
    return 0;
}
static int powercuts(void)
{
    fixture();REQUIRE(proj_pack(&oldwire,&q)&&st_save(OBJ_PROJECT0,&oldwire,3840)==0);
    q.t[0].p[P_GCUT]=-37;REQUIRE(proj_pack(&wire,&q));
    limit_lo=st_sector(OBJ_PROJECT0,0);limit_hi=limit_lo+8192;
    static uint8_t snapshot[8192],loaded[3840];memcpy(snapshot,nor+limit_lo,sizeof snapshot);
    bk_read_hook=fault_read;bk_erase_hook=fault_erase;bk_prog_hook=fault_prog;
    for(uint32_t direction=0;direction<2;direction++) {
    if(direction){REQUIRE(st_save(OBJ_PROJECT0,&oldwire,3840)==0);memcpy(snapshot,nor+limit_lo,sizeof snapshot);}
    for(uint32_t cut=0;cut<=3872;cut++) {
        memcpy(nor+limit_lo,snapshot,sizeof snapshot);budget=cut;armed=0;
        int rc=st_save(OBJ_PROJECT0,&wire,3840);budget=-1;
        if(cut==3872)REQUIRE(rc==0);
        REQUIRE(st_load(OBJ_PROJECT0,loaded,sizeof loaded)==3840);
        REQUIRE(!memcmp(loaded,&oldwire,3840)||!memcmp(loaded,&wire,3840));
        if(cut==3872)REQUIRE(!memcmp(loaded,&wire,3840));
    }
    for(uint32_t cut=0;cut<=4096;cut+=256){memcpy(nor+limit_lo,snapshot,sizeof snapshot);erase_cut=cut;REQUIRE(st_save(OBJ_PROJECT0,&wire,3840)!=0);erase_cut=-1;REQUIRE(st_load(OBJ_PROJECT0,loaded,3840)==3840&&!memcmp(loaded,&oldwire,3840));}
    for(uint32_t mode=0;mode<2;mode++) {
        memcpy(nor+limit_lo,snapshot,sizeof snapshot);ack_loss=mode==0;readback_loss=mode==1;armed=0;
        REQUIRE(st_save(OBJ_PROJECT0,&wire,3840)!=0);ack_loss=readback_loss=armed=0;
        REQUIRE(st_load(OBJ_PROJECT0,loaded,3840)==3840&&!memcmp(loaded,&wire,3840));
    }
    }
    REQUIRE(!bad_write);bk_read_hook=0;bk_erase_hook=0;bk_prog_hook=0;
    printf("program cuts=7746 erase cuts=34 lost acknowledgements/readback failures=4\n");return 1;
}
int main(int argc,char **argv)
{
    run_test("losslessness gate: legacy metadata incl inactive steps",proof);
    if(argc>1&&!strcmp(argv[1],"--proof-only"))return failures!=0;
    run_test("all fields, GRAIN USR8, inactive chain bytes and exact layout",roundtrip);
    run_test("every step bit at every position, guards and omitted-bit rejection",bit_basis);
    run_test("all 26 SAMPLE/GRAIN source selections through real save/load",all_sources);
    run_test("all active chain rows and reserved header bytes",active_chain);
    run_test("actual save, backup LIST/PUT, MIDI/FM6/motion and repeated loads",save_backup);
    run_test("legacy import, verbatim cache, recovery and backup conversion",legacy);
    run_test("FUN1..FUN6 exact cached lengths, backup conversion and old RAM snapshots",legacy_raw);
    run_test("maximal valid state, 3840-byte canaries and oversize/range rejection",maximum);
    run_test("truncation, corruption, semantic errors and no failed-restore erase",malformed);
    run_test("A/B power loss, bounded writes, acknowledgement/readback failures",powercuts);
    printf("project codec: %d passed, %d failed\n",passed,failures);return failures!=0;
}
